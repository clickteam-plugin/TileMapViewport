#include "Common.h"
#include "Helpers.h"

// Wraps a pair of two numbers representing a low and a high boundary whose difference must be retained
// Returns true if there was no split. Otherwise, split contains the offset from a where the split must be performed
inline int signmodPair(int& a, int&b, unsigned* split, int room)
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
	if (split && b >= room)
	{	
		*split = room - a;
		return false;
	}

	return true;
}

// Returns true if the given on-screen rectangle collides with the given layer region using the given tileset (for pixel-testing)
bool checkRectangleOverlap(LPRDATA rdPtr, Layer& layer, Tileset& tileset, Rect rect)
{
	if (!rdPtr->p)
		return false;

	bool fineColl = rdPtr->fineColl;

	// Store some frequenlayerY used values
	int tileWidth = layer.settings.tileWidth;
	int tileHeight = layer.settings.tileHeight;
	int layerWidth = layer.getWidth();
	int layerHeight = layer.getHeight();
	int layerPxWidth = layerWidth * tileWidth;
	int layerPxHeight = layerHeight * tileHeight;

	// Compute layer position on screen
	int layerX = layer.getScreenX(rdPtr->cameraX) + rdPtr->rHo.hoRect.left;
	int layerY = layer.getScreenY(rdPtr->cameraY) + rdPtr->rHo.hoRect.top;

	// Not overlapping visible part, exit
	if (!rdPtr->outsideColl)
	{
		if (rect.x2 < rdPtr->rHo.hoX - rdPtr->collMargin.left
		||	rect.y2 < rdPtr->rHo.hoY - rdPtr->collMargin.top
		||	rect.x1 > rdPtr->rHo.hoX + rdPtr->rHo.hoImgWidth + rdPtr->collMargin.right
		||	rect.y1 > rdPtr->rHo.hoY + rdPtr->rHo.hoImgHeight + rdPtr->collMargin.bottom)
			return false;
	}

	// Make object coordinates relative to layer's origin
	rect.moveBy(-rdPtr->cameraX-layerX, -rdPtr->cameraY-layerY);

	// Get the tiles that the object overlaps

	// Stack of the tile regions that have to be examined
	// Whenever an object is on the edge
	// The rectangle stack stores the pixel rectangle's subrect for the according region
	Rect rectStack[5];
	unsigned rectCount = 1;

	// The first region to check is the entire object's overlapped tiles
	rectStack[0] = rect;

	// Until there are no more rectangles to check...
	while (rectCount--)
	{
		// This should NEVER EVER happen, so if it really does, make the user report this issue immediately.
		if (rectCount >= 5)
			MessageBox(0, "Tile Map has a problem. Please post this in the forum thread.", "Rectangle stack overflow!", 0);

		Rect subRect = rectStack[rectCount];

		// Wrap the tiles if necessary
		unsigned split = 0xffffffff;
		if (layer.settings.wrapX)
		{
			if (!signmodPair(subRect.x1, subRect.x2, &split, layerPxWidth))
			{
				// Create left half
				Rect& split1 = rectStack[rectCount++];
				split1 = subRect;
				split1.x2 = split1.x1 + split - 1;

				// Create right half
				Rect& split2 = rectStack[rectCount++];
				split2 = subRect;
				split2.x1 = split1.x2 + 1;

				// Fail-safe: Modulo again. From what I know, this is unnecessary.
				//signmodPair(split1.x1, split1.x2, 0, layerPxWidth);
				//signmodPair(split2.x1, split2.x2, 0, layerPxWidth);
			}
		}
		if (layer.settings.wrapY)
		{
			if (!signmodPair(subRect.y1, subRect.y2, &split, layerPxHeight))
			{
				// Create left half
				Rect& split1 = rectStack[rectCount++];
				split1 = subRect;
				split1.y2 = split1.y1 + split - 1;

				// Create right half
				Rect& split2 = rectStack[rectCount++];
				split2 = subRect;
				split2.y1 = split1.y2 + 1;
			}
		}

		// Split occured -> new regions to check added. This one can now be ignored
		if (split != 0xffffffff)
			continue;

		// Calculate the tiles that this rectangle overlaps
		int x1 = floordiv(subRect.x1, tileWidth);
		int y1 = floordiv(subRect.y1, tileHeight);
		int x2 = floordiv(subRect.x2 - 1, tileWidth);
		int y2 = floordiv(subRect.y2 - 1, tileHeight);

		// Limit candidates to possibly existing tiles
		x1 = max(0, min(x1, layerWidth - 1));
		x2 = max(0, min(x2, layerWidth - 1));
		y1 = max(0, min(y1, layerHeight - 1));
		y2 = max(0, min(y2, layerHeight - 1));

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
					Rect tileBounds;
					tileBounds.x1 = tileWidth * x;
					tileBounds.y1 = tileHeight * y;
					tileBounds.x2 = tileBounds.x1 + tileWidth;
					tileBounds.y2 = tileBounds.y1 + tileHeight;

					// Get pixel offset of tile in tileset
					int tilesetX = tile->x * tileWidth;
					int tilesetY = tile->y * tileHeight;

					// Get intersection box (relative to tile)
					Rect intersect;
					intersect.x1 = max(subRect.x1, tileBounds.x1) - tileBounds.x1 + tilesetX;
					intersect.y1 = max(subRect.y1, tileBounds.y1) - tileBounds.y1 + tilesetY;
					intersect.x2 = min(subRect.x2, tileBounds.x2) - tileBounds.x1 + tilesetX;
					intersect.y2 = min(subRect.y2, tileBounds.y2) - tileBounds.y1 + tilesetY;

					cSurface* surface = tileset.surface;
					bool alpha = surface->HasAlpha() != 0;

					// Check by alpha channel
					if (alpha)
					{
						cSurface* alphaSurf = rdPtr->cndAlphaSurf;

						for (int iX = intersect.x1; iX < intersect.x2; ++iX)
							for (int iY = intersect.y1; iY < intersect.y2; ++iY)
								if (alphaSurf->GetPixelFast8(iX, iY) > 0)
									return true;
					}
					// Check by transparent color
					else
					{
						COLORREF transpCol = surface->GetTransparentColor();

						if (tilesetX == 240)
							printf("%d,%d,%d,%d\n", intersect.x1, intersect.y1, intersect.x2, intersect.y2);

						for (int iX = intersect.x1; iX < intersect.x2; ++iX)
							for (int iY = intersect.y1; iY < intersect.y2; ++iY)
								if (surface->GetPixelFast(iX, iY) != transpCol)
									return true;
					}
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
//		signmodPair(rect.x1, rect.x2, split, layerWidth*tileWidth);
//	if (layer.settings.wrapY)
//		signmodPair(rect.y1, rect.y2, split, layerHeight*tileHeight);
//}
