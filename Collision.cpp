#include "Common.h"

// Divide x by d, round towards -infinity, always
inline int floordiv(int x, int d)
{
	// Various solutions from the Internet didn't work. This one makes sense and the efficiency is negligible
	if (x < 0)
	{
		int unaligned = x;
		x = 0;

		while (x > unaligned)
			x -= d;
	}

	return x / d;
}

// Modulo that loops negative numbers as well
inline int signmod(int x, int room)
{
	while (x < 0)
		x += room;

	return x %= room;
}

// Wraps a pair of two numbers representing a low and a high boundary whose difference must be retained
// Returns true if there was no split. Otherwise, split contains the offset from a where the split must be performed
inline int signmodPair(int& a, int&b, unsigned& split, int room)
{
	// Wrapping right/bottom
	if (a >= room)
	{
		int old = a;
		a %= room;
		b = b - old + a;
	}

	// Wrapping left/top
	while (a < 0)
	{
		a += room;
		b += room;
	}

	// Crossing the line... need to split
	if (b >= room)
	{	
		split = room - a;
		return false;
	}

	return true;
}

// Like checkObjectOverlapRegion, but with the whole layer as region...
bool checkObjectOverlap(LPRDATA rdPtr, Layer& layer, Tileset& tileset, LPHO obj)
{
	RECT region;
	region.left = 0;
	region.top = 0;
	region.right = layer.getWidth() - 1;
	region.bottom = layer.getHeight() - 1;
	return checkObjectOverlapRegion(rdPtr, layer, tileset, region, obj);
}

// Returns true if the given object collides with the given layer region using the given tileset (for pixel-testing)
// regionFlag is for stack overflow prevention: 1 bit set = X split already performed, 2 bit set = Y split already performed
bool checkObjectOverlapRegion(LPRDATA rdPtr, Layer& layer, Tileset& tileset, const RECT& region, LPHO obj, int regionFlag, int regionObjPos)
{
	if (!rdPtr->p)
		return false;

	// Store some frequenlayerY used values
	int tileWidth = layer.settings.tileWidth;
	int tileHeight = layer.settings.tileHeight;
	int layerWidth = layer.getWidth();
	int layerHeight = layer.getHeight();

	// Compute layer position on screen
	int layerX = layer.getScreenX(rdPtr->cameraX) + rdPtr->rHo.hoRect.left;
	int layerY = layer.getScreenY(rdPtr->cameraY) + rdPtr->rHo.hoRect.top;

	// Get object coordinates
	int objX1 = obj->hoX - obj->hoImgXSpot;
	int objY1 = obj->hoY - obj->hoImgYSpot;

	// Not overlapping visible part, exit
	if (!regionFlag && !rdPtr->outsideColl)
	{
		if (objX1 + obj->hoImgWidth < rdPtr->rHo.hoX - rdPtr->collMargin.left
		||	objY1 + obj->hoImgHeight < rdPtr->rHo.hoY - rdPtr->collMargin.top
		||	objX1 > rdPtr->rHo.hoX + rdPtr->rHo.hoImgWidth + rdPtr->collMargin.right
		||	objY1 > rdPtr->rHo.hoY + rdPtr->rHo.hoImgHeight + rdPtr->collMargin.bottom)
			return false;
	}

	// Convert to on-screen coordinates
	objX1 -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
	objY1 -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;

	int objX2 = objX1 + obj->hoImgWidth;
	int objY2 = objY1 + obj->hoImgHeight;

	// Make object coordinates relative to layer's origin
	objX1 -= layerX;
	objY1 -= layerY;
	objX2 -= layerX;
	objY2 -= layerY;

	// Get the tiles that the object overlaps
	int x1 = floordiv(objX1, tileWidth);
	int y1 = floordiv(objY1, tileHeight);
	int x2 = floordiv(objX2 - 1, tileWidth);
	int y2 = floordiv(objY2 - 1, tileHeight);

	// To calculate the wrapped object coordinate
	int oldX1 = x1, oldY1 = y1;


	// Wrap the tiles if necessary
	if (layer.settings.wrapX && (regionFlag & 1) == 0)
	{
		unsigned split;
		if (!signmodPair(x1, x2, split, layerWidth))
		{
			RECT region = {x1, y1, x2, y2};

			// Create left half
			RECT lSplit = region;
			lSplit.right = region.left + split - 1;

			// Create right half
			RECT rSplit = region;
			rSplit.left = lSplit.right + 1;
			rSplit.left %= layerWidth;
			rSplit.right %= layerWidth;

			printf("T %d, %d, %d, %d\n", region.left, region.top, region.right, region.bottom);
			printf("L %d, %d, %d, %d\n", lSplit.left, lSplit.top, lSplit.right, lSplit.bottom);
			printf("R %d, %d, %d, %d\n", rSplit.left, rSplit.top, rSplit.right, rSplit.bottom);

			return checkObjectOverlapRegion(rdPtr, layer, tileset, lSplit, obj, regionFlag | 1, objX1 - (oldX1 - x1) * tileWidth)
				|| checkObjectOverlapRegion(rdPtr, layer, tileset, rSplit, obj, regionFlag | 1, objX1 - (oldX1 - x1) * tileWidth);
		}
	}
	if (layer.settings.wrapY && (regionFlag & 2) == 0)
	{
		unsigned split;
		if (!signmodPair(y1, y2, split, layerHeight))
			printf("TY split %d ... ", split);
	}



	// Unwrap  object coordinates (according to wrapped tiles)
	if (regionFlag & 1)
	{
		objX1 = regionObjPos;
		objX2 = regionObjPos + obj->hoImgWidth;
	}
	else
	{
		objX1 -= (oldX1 - x1) * tileWidth;
		objX2 -= (oldX1 - x1) * tileWidth;
	}
	objY1 -= (oldY1 - y1) * tileHeight;
	objY2 -= (oldY1 - y1) * tileHeight;


	// Limit candidates to possibly overlapping tiles
	x1 = max(region.left, min(region.right, x1));
	x2 = max(region.left, min(region.right, x2));
	y1 = max(region.top, min(region.bottom, y1));
	y2 = max(region.top, min(region.bottom, y2));

	// Check for any overlapping tile
	bool fineColl = rdPtr->fineColl;
	for (int x = x1; x <= x2; ++x)
	{
		for (int y = y1; y <= y2; ++y)
		{
			Tile* tile = layer.getTile(x, y);
			if (tile->id != Tile::EMPTY)
			{
				// Bounding box collisions - we're done
				if (!fineColl)
					return true;

				// Get bounding box of tile
				int tileX1 = tileWidth * x;
				int tileY1 = tileHeight * y;
				int tileX2 = tileWidth * (x + 1);
				int tileY2 = tileHeight * (y + 1);

				// Get intersection box (relative to tile)
				int intersectX1 = max(objX1, tileX1) - tileX1;
				int intersectY1 = max(objY1, tileY1) - tileY1;
				int intersectX2 = min(objX2, tileX2) - tileX1;
				int intersectY2 = min(objY2, tileY2) - tileY1;

				// Get position of tile in tileset
				int tilesetX = tile->x * tileWidth;
				int tilesetY = tile->y * tileHeight;

				cSurface* surface = tileset.surface;
				bool alpha = surface->HasAlpha() != 0;

				// Check by alpha channel
				if (alpha)
				{
					cSurface* alphaSurf = rdPtr->cndAlphaSurf;

					for (int iX = intersectX1; iX < intersectX2; ++iX)
						for (int iY = intersectY1; iY < intersectY2; ++iY)
							if (alphaSurf->GetPixelFast8(tilesetX + iX, tilesetY + iY) > 0)
								return true;
				}
				// Check by transparent color
				else
				{
					COLORREF transpCol = surface->GetTransparentColor();

					for (int iX = intersectX1; iX < intersectX2; ++iX)
						for (int iY = intersectY1; iY < intersectY2; ++iY)
							if (surface->GetPixelFast(tilesetX + iX, tilesetY + iY) != transpCol)
								return true;
				}
			}
		}
	}

	return false;
}

// Returns true if the given coordinate is solid
bool checkPixelSolid(LPRDATA rdPtr, Layer& layer, Tileset& tileset, int pixelX, int pixelY)
{
	// Store tile size
	int tileWidth = layer.settings.tileWidth;
	int tileHeight = layer.settings.tileHeight;

	// Compute layer position on screen
	int layerX = layer.getScreenX(rdPtr->cameraX) + rdPtr->rHo.hoRect.left;
	int layerY = layer.getScreenY(rdPtr->cameraY) + rdPtr->rHo.hoRect.top;

	// Get layer size in px
	int layerWidth = layer.getWidth() * tileWidth;
	int layerHeight = layer.getHeight() * tileHeight;

	// Not overlapping visible part, exit
	if (!rdPtr->outsideColl)
	{
		if (pixelX < rdPtr->rHo.hoX
		|| pixelY < rdPtr->rHo.hoY
		|| pixelX > rdPtr->rHo.hoX + rdPtr->rHo.hoImgWidth
		|| pixelY > rdPtr->rHo.hoY + rdPtr->rHo.hoImgHeight)
			return false;
	}

	// Convert to on-screen coordinates
	pixelX -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
	pixelY -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;

	// Get the tile that the object overlaps
	int tilePosX = floordiv(pixelX - layerX, tileWidth);
	int tilePosY = floordiv(pixelY - layerY, tileHeight);

	// Ensure that the tiles are in the layer
	int width = layer.getWidth();
	int height = layer.getHeight();

	// Nothing to do if the object is not within the tile area
	if (tilePosX < 0 || tilePosY < 0 || tilePosX >= width || tilePosY >= height)
		return false;

	// Make object coordinates relative to layer's origin
	pixelX -= layerX;
	pixelY -= layerY;

	// Check overlapping tile
	Tile* tile = layer.getTile(tilePosX, tilePosY);

	if (tile->id == Tile::EMPTY)
		return false;

	// No fine collision? We're done...
	if (!rdPtr->fineColl)
		return true;

	// Get the pixel in the tile that we have to check...
	int tilesetX = pixelX + (tile->x - tilePosX) * tileWidth;
	int tilesetY = pixelY + (tile->y - tilePosY) * tileHeight;

	cSurface* surface = tileset.surface;

	if (!surface)
		return false;

	bool alpha = surface->HasAlpha() != 0;

	if (alpha)
	{
		cSurface* alphaSurf = surface->GetAlphaSurface();
		bool result = alphaSurf->GetPixelFast8(tilesetX, tilesetY) != 0;
		surface->ReleaseAlphaSurface(alphaSurf);
		return result;
	}

	return surface->GetPixelFast(tilesetX, tilesetY) != surface->GetTransparentColor();
}


// UNUSED code that will probably never be reused!
//if (rdPtr->outsideColl)
//{
//	if (layer.settings.wrapX)
//		signmodPair(objX1, objX2, split, layerWidth*tileWidth);
//	if (layer.settings.wrapY)
//		signmodPair(objY1, objY2, split, layerHeight*tileHeight);
//}
