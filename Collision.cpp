#include "Common.h"
#include "Helpers.h"

// Returns true if the given on-screen rectangle collides with the given layer region using the given tileset (for pixel-testing)
bool checkRectangleOverlap(LPRDATA rdPtr, Layer& layer, Tileset& tileset, Rect rect)
{
	if (!rdPtr->p)
		return false;

	bool fineColl = rdPtr->fineColl;

	// Store some frequently used values
	float zoom = rdPtr->zoom;
	int tileWidth = layer.settings.tileWidth;
	int tileHeight = layer.settings.tileHeight;
	float renderTileWidth = tileWidth * zoom;
	float renderTileHeight = tileHeight * zoom;
	int layerWidth = layer.getWidth();
	int layerHeight = layer.getHeight();
	int layerPxWidth = layerWidth * renderTileWidth;
	int layerPxHeight = layerHeight * renderTileHeight;

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
	rect.moveBy(-(layerX + rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX), -(layerY + rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY));

	// Get the tiles that the object overlaps

	// Stack of the tile regions that have to be examined
	// Whenever an object is on the edge
	// The rectangle stack stores the pixel rectangle's rect for the according region
	const int RECT_MAX = 8;
	static Rect rectStack[RECT_MAX];
	unsigned rectCount = 1;

	// The first region to check is the entire object's overlapped tiles
	rectStack[0] = rect;

	// Until there are no more rectangles to check...
	while (rectCount--)
	{
		// This should NEVER EVER happen, so if it really does, make the user report this issue immediately.
		if (rectCount > RECT_MAX)
			MessageBox(0, "Tile Map has a problem. Please post this in the forum thread.", "Rectangle stack overflow!", 0);

		// Get the most important rectangle
		rect = rectStack[rectCount];

		// Wrap the tiles if necessary
		unsigned splitOffset = 0;
		if (layer.settings.wrapX)
		{
			if (!signmodPair(rect.x1, rect.x2, &splitOffset, layerPxWidth))
			{
				// Create left half
				Rect& split1 = rectStack[rectCount++];
				split1 = rect;
				split1.x2 = split1.x1 + splitOffset - 1;

				if (rectCount < RECT_MAX)
				{
					// Create right half
					Rect& split2 = rectStack[rectCount++];
					split2 = rect;
					split2.x1 = split1.x2 + 1;
				}

				// Fail-safe: Modulo again. From what I know, this is unnecessary.
				//signmodPair(split1.x1, split1.x2, 0, layerPxWidth);
				//signmodPair(split2.x1, split2.x2, 0, layerPxWidth);
			}
		}
		if (layer.settings.wrapY)
		{
			if (!signmodPair(rect.y1, rect.y2, &splitOffset, layerPxHeight))
			{
				// Create top half
				Rect& split1 = rectStack[rectCount++];
				split1 = rect;
				split1.y2 = split1.y1 + splitOffset - 1;

				if (rectCount < RECT_MAX)
				{
					// Create bottom half
					Rect& split2 = rectStack[rectCount++];
					split2 = rect;
					split2.y1 = split1.y2 + 1;
				}
			}
		}

		// Split occured -> new regions to check added. This one can now be ignored
		if (splitOffset)
			continue;

		// Calculate the tiles that this rectangle overlaps
		int x1 = floordiv<float>(rect.x1, renderTileWidth);
		int y1 = floordiv<float>(rect.y1, renderTileHeight);
		int x2 = floordiv<float>(rect.x2 - 1, renderTileWidth);
		int y2 = floordiv<float>(rect.y2 - 1, renderTileHeight);

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
					// Apply overlap filters
					bool filtered = false;
					for (unsigned f = 0; f < rdPtr->ovlpFilterCount; ++f)
					{
						filtered = true;
						OVERLAPFLT& filter = rdPtr->ovlpFilters[f];

						int value = 0;
						const SubLayer* subLayer;
						switch (filter.type)
						{
							case OFT_SUBLAYER:
								
								if (subLayer  = rdPtr->sublayerCache[filter.param])
								{
									subLayer->getCellSafe(x, y, &value);
									if (value == filter.value)
										filtered = false;
								}
								break;
							
							case OFT_TILESETX:

								if (tile->x == filter.value)
									filtered = false;
								break;

							case OFT_TILESETY:

								if (tile->y == filter.value)
									filtered = false;
								break;

							case OFT_TILESETRANGE:

								TileRange tr = *(TileRange*)&filter.value;
								if (tr.isWithin(*tile))
									filtered = false;
								break;
						}
					}

					// This tile is uninteresting for the given filter
					if (filtered)
						continue;

					// Bounding box collisions - we're done
					if (!fineColl)
						return true;

					// Get bounding box of tile
					Rect tileBounds;
					tileBounds.x1 = renderTileWidth * x;
					tileBounds.y1 = renderTileHeight * y;
					tileBounds.x2 = tileBounds.x1 + renderTileWidth;
					tileBounds.y2 = tileBounds.y1 + renderTileHeight;

					// Get pixel offset of tile in tileset
					int tilesetX = tile->x * tileWidth;
					int tilesetY = tile->y * tileHeight;

					// Get intersection box (relative to tile)
					Rect intersect;
					intersect.x1 = max(rect.x1, tileBounds.x1) - tileBounds.x1 + tilesetX;
					intersect.y1 = max(rect.y1, tileBounds.y1) - tileBounds.y1 + tilesetY;
					intersect.x2 = min(rect.x2, tileBounds.x2) - tileBounds.x1 + tilesetX;
					intersect.y2 = min(rect.y2, tileBounds.y2) - tileBounds.y1 + tilesetY;

					intersect.x1 /= zoom;
					intersect.y1 /= zoom;
					intersect.x2 /= zoom;
					intersect.y2 /= zoom;

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
	int layerWidth = layer.getWidth();
	int layerHeight = layer.getHeight();

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
	pixelX -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX + layerX;
	pixelY -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY + layerY;

	// Get the tile that the object overlaps
	int tilePosX = floordiv<float>(pixelX, tileWidth * rdPtr->zoom);
	int tilePosY = floordiv<float>(pixelY, tileHeight * rdPtr->zoom);

	// Limit X coordinate
	if (layer.settings.wrapX)
	{
		tilePosX = signmod(tilePosX, layerWidth);
		pixelX = signmod(pixelX, layerWidth * tileWidth * rdPtr->zoom);
	}
	else if (tilePosX < 0 || tilePosX >= layerWidth)
		return false;

	// Limit Y coordinate
	if (layer.settings.wrapY)
	{
		tilePosY = signmod(tilePosY, layerHeight);
		pixelY = signmod(pixelY, layerHeight * tileHeight * rdPtr->zoom);
	}
	else if (tilePosY < 0 || tilePosY >= layerHeight)
		return false;

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
