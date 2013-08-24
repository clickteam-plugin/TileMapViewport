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
inline bool signmodPair(int& a, int&b, int room)
{
	// [a,b] is on the edge of the room, there's nothing we can do
	if ((a < 0 && b >= 0) || (a < room && b >= room))
		return false;

	while (a < 0)
	{
		a += room;
		b += room;
	}

	if (a >= room)
	{
		int old = a;
		a %= room;
		b = b - old + a;
	}
	
	return true;
}

// Returns true if the given object collides with the given layer using the given tileset (for pixel-testing)
bool checkObjectOverlap(LPRDATA rdPtr, Layer& layer, Tileset& tileset, LPHO obj)
{
	if (!rdPtr->p)
		return false;
	
	// Store some frequently used values
	int tileWidth = layer.settings.tileWidth;
	int tileHeight = layer.settings.tileHeight;
	int layerWidth = layer.getWidth();
	int layerHeight = layer.getHeight();

	// Compute layer position on screen
	int tlX = layer.getScreenX(rdPtr->cameraX) + rdPtr->rHo.hoRect.left;
	int tlY = layer.getScreenY(rdPtr->cameraY) + rdPtr->rHo.hoRect.top;

	// Get object coordinates
	int objX1 = obj->hoX - obj->hoImgXSpot;
	int objY1 = obj->hoY - obj->hoImgYSpot;

	// Not overlapping visible part, exit
	if (!rdPtr->outsideColl)
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

	// If we want to collide outside of the viewport, we have to make sure the object coords stay in the layer region
	if (rdPtr->outsideColl)
	{
		if (layer.settings.wrapX)
			objX1 = signmod(objX1, layerWidth*tileWidth);
		if (layer.settings.wrapY)
			objY1 = signmod(objY1, layerHeight*tileHeight);
	}

	int objX2 = objX1 + obj->hoImgWidth;
	int objY2 = objY1 + obj->hoImgHeight;

	// Get the tiles that the object overlaps
	int x1 = floordiv(objX1 - tlX, tileWidth);
	int y1 = floordiv(objY1 - tlY, tileHeight);
	int x2 = floordiv(objX2 - tlX - 1, tileWidth);
	int y2 = floordiv(objY2 - tlY - 1, tileHeight);

	// Wrap the coordinates if necessary
	if (layer.settings.wrapX)
	{
		if (!signmodPair(x1, x2, layerWidth))
			printf("Horizontal wrap edge detected! TODO!\n");
	}
	if (layer.settings.wrapY)
	{
		if (!signmodPair(y1, y2, layerHeight))
			printf("Vertical wrap edge detected! TODO!\n");
	}

	// TODO, I guess: Collisions on the edge of a layer wrap are not properly implemented

	// Nothing to do if the object is not within the tile area
	if (x1 >= layerWidth || y1 >= layerHeight || x2 < 0 || y2 < 0)
		return false;

	// Limit candidates to possibly overlapping tiles
	x1 = max(0, x1);
	x2 = min(layerWidth-1, x2);
	y1 = max(0, y1);
	y2 = min(layerHeight-1, y2);

	// Make object coordinates relative to layer's origin
	objX1 -= tlX;
	objY1 -= tlY;
	objX2 -= tlX;
	objY2 -= tlY;

	if (layer.settings.wrapX)
	{
		if (!signmodPair(objX1, objX2, layerWidth*tileWidth))
			printf("Horizontal wrap edge detected! TODO!\n");
		objX1 = signmod(objX1, layerWidth*tileWidth);
		objX2 = signmod(objX2, layerWidth*tileWidth);
	}
	if (layer.settings.wrapY)
	{
		if (!signmodPair(objY1, objY2, layerHeight*tileHeight))
			printf("Vertical wrap edge detected! TODO!\n");
	}

	bool fineColl = rdPtr->fineColl;

	// Check for any overlapping tile
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
	int tlX = layer.getScreenX(rdPtr->cameraX) + rdPtr->rHo.hoRect.left;
	int tlY = layer.getScreenY(rdPtr->cameraY) + rdPtr->rHo.hoRect.top;

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
	int tilePosX = floordiv(pixelX - tlX, tileWidth);
	int tilePosY = floordiv(pixelY - tlY, tileHeight);

	// Ensure that the tiles are in the layer
	int width = layer.getWidth();
	int height = layer.getHeight();

	// Nothing to do if the object is not within the tile area
	if (tilePosX < 0 || tilePosY < 0 || tilePosX >= width || tilePosY >= height)
		return false;

	// Make object coordinates relative to layer's origin
	pixelX -= tlX;
	pixelY -= tlY;

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