// ============================================================================
//
// This file contains the actions, conditions and expressions your object uses
// 
// ============================================================================

#include "Common.h"
#include "Paramacro.h"

// ============================================================================
//
// CONDITIONS
// 
// ============================================================================

/* Divide x by d, round towards -infinity, always */
inline int floordiv(int x, int d)
{
	/* Various solutions from the Internet didn't work. This one makes sense and the efficiency is negligible */
	if (x < 0)
	{
		int unaligned = x;
		x = 0;

		while (x > unaligned)
			x -= d;
	}

	return x / d;
}

/* NOTE: Requires rdPtr->cndTileset to be set accordingly if fineColl is true! */
long cndObjOverlapsLayer(LPRDATA rdPtr, LPRO runObj, long layerParam)
{
	if (!rdPtr->p)
		return false;

	Layer* layer = (Layer*)layerParam;
	LPHO obj = (LPHO)runObj;

	/* Required for fine collisions */
	Tileset* tileset = rdPtr->cndTileset;
	
	/* Store tile size (we'll need it often) */
	int tileWidth = rdPtr->p->tileWidth;
	int tileHeight = rdPtr->p->tileHeight;

	/* Compute layer position on screen */
	int tlX = getLayerX(rdPtr, layer) + rdPtr->rHo.hoRect.left;
	int tlY = getLayerY(rdPtr, layer) + rdPtr->rHo.hoRect.top;

	/* Get object coordinates */
	int objX = obj->hoX - obj->hoImgXSpot;
	int objY = obj->hoY - obj->hoImgYSpot;

	/* Get layer size in px */
	int layerWidth = layer->width * tileWidth;
	int layerHeight = layer->height * tileHeight;

	/* Not overlapping visible part, exit */
	if (!rdPtr->outsideColl)
	{
		if (objX + obj->hoImgWidth < rdPtr->rHo.hoX
		|| objY + obj->hoImgHeight < rdPtr->rHo.hoY
		|| objX > rdPtr->rHo.hoX + rdPtr->rHo.hoImgWidth
		|| objY > rdPtr->rHo.hoY + rdPtr->rHo.hoImgHeight)
			return false;
	}

	/* Convert to on-screen coordinates */
	objX -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
	objY -= rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;

	/* Wrap the coordinates if necessary */
	if (layer->wrapX)
	{
		while (objX < 0)
			objX += layerWidth;

		objX %= layerWidth; 
	}
	if (layer->wrapY)
	{
		while (objY < 0)
			objY += layerHeight;

		objY %= layerHeight; 
	}

	/* Get bounding box of object */
	int objX1 = objX;
	int objY1 = objY;
	int objX2 = objX + obj->hoImgWidth;
	int objY2 = objY + obj->hoImgHeight;

	/* Get the tiles that the object overlaps */
	int x1 = floordiv(objX1 - tlX, tileWidth);
	int y1 = floordiv(objY1 - tlY, tileHeight);
	int x2 = floordiv(objX2 - tlX - 1, tileWidth);
	int y2 = floordiv(objY2 - tlY - 1, tileHeight);


	/* Ensure that the tiles are in the layer */
	int width = layer->width;
	int height = layer->height;


	/* Nothing to do if the object is not within the tile area */
	if (x1 >= width || y1 >= height || x2 < 0 || y2 < 0)
		return false;

	/* Limit candidates to possibly overlapping tiles */
	x1 = max(0, x1);
	x2 = min(width-1, x2);
	y1 = max(0, y1);
	y2 = min(height-1, y2);

	/* Make object coordinates relative to layer's origin */
	objX1 -= tlX;
	objY1 -= tlY;
	objX2 -= tlX;
	objY2 -= tlY;

	bool fineColl = rdPtr->fineColl;

	/* Check for any overlapping tile */
	for (int x = x1; x <= x2; ++x)
	{
		for (int y = y1; y <= y2; ++y)
		{
			Tile* tile = layer->get(x, y);
			if (tile->x != 0xff && tile->y != 0xff)
			{
				/* Bounding box collisions - we're done */
				if (!fineColl)
					return true;

				/* Get bounding box of tile */
				int tileX1 = tileWidth * x;
				int tileY1 = tileHeight * y;
				int tileX2 = tileWidth * (x + 1);
				int tileY2 = tileHeight * (y + 1);

				/* Get intersection box (relative to tile) */
				int intersectX1 = max(objX1, tileX1) - tileX1;
				int intersectY1 = max(objY1, tileY1) - tileY1;
				int intersectX2 = min(objX2, tileX2) - tileX1;
				int intersectY2 = min(objY2, tileY2) - tileY1;

				/* Get position of tile in tileset */
				int tilesetX = tile->x * tileWidth;
				int tilesetY = tile->y * tileHeight;

				cSurface* surface = tileset->surface;
				bool alpha = surface->HasAlpha() != 0;

				/* Check by alpha channel */
				if (alpha)
				{
					cSurface* alphaSurf = rdPtr->cndAlphaSurf;

					for (int iX = intersectX1; iX < intersectX2; ++iX)
						for (int iY = intersectY1; iY < intersectY2; ++iY)
							if (alphaSurf->GetPixelFast8(tilesetX + iX, tilesetY + iY) > 0)
								return true;
				}
				/* Check by transparent color */
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

CONDITION(
	/* ID */			0,
	/* Name */			"%o: %0 is overlapping layer %1",
	/* Flags */			EVFLAGS_ALWAYS|EVFLAGS_NOTABLE,
	/* Params */		(2, PARAM_OBJECT,"Object", PARAM_NUMBER,"Layer index")
) {
	if (!rdPtr->p)
		return false;

	unsigned int id = (unsigned)param2;

	if (id >= rdPtr->p->layers->size())
		return false;

	Layer* layer = &(*rdPtr->p->layers)[id];
	if (!layer->isValid())
		return false;


	/* Get layer's collision tileset */
	unsigned char tilesetID = (layer->collision != 0xff) ? layer->collision : layer->tileset;
	if (tilesetID >= rdPtr->p->tilesets->size())
		return false;

	rdPtr->cndTileset = &(*rdPtr->p->tilesets)[tilesetID];

	/* Get tileset's settings */
	cSurface* surface = rdPtr->cndTileset->surface;
	if (!surface)
		return false;

	/* Tile box collision */
	long overlapping = 0;
	if (!rdPtr->fineColl)
	{
		/* Perform overlap test */
		overlapping = ProcessCondition(rdPtr, param1, (long)layer, cndObjOverlapsLayer);
	}
	/* Fine collisions need some extra work... */
	else
	{
		/* Prepare surface buffer for reading */
		BYTE* buff;
		if (surface->HasAlpha())
		{
			rdPtr->cndAlphaSurf = surface->GetAlphaSurface();
			buff = rdPtr->cndAlphaSurf->LockBuffer();
		}
		else
		{
			buff = surface->LockBuffer();
		}

		/* Perform overlap test */
		overlapping = ProcessCondition(rdPtr, param1, (long)layer, cndObjOverlapsLayer);

		/* Done, now unlock buffers */
		if (surface->HasAlpha())
		{
			rdPtr->cndAlphaSurf->UnlockBuffer(buff);
			surface->ReleaseAlphaSurface(rdPtr->cndAlphaSurf);
		}
		else
		{
			surface->UnlockBuffer(buff);
		}
	}

	return overlapping;
}

CONDITION(
	/* ID */			1,
	/* Name */			"%o: On tile",
	/* Flags */			0,
	/* Params */		(0)
) {
	return true;
}


//CONDITION(
//	/* ID */			1,
//	/* Name */			"%o: %0 is overlapping layer %1 (pixel-perfect)",
//	/* Flags */			EVFLAGS_ALWAYS,
//	/* Params */		(2, PARAM_OBJECT,"Object", PARAM_NUMBER,"Layer index")
//) {
//	long obj = param1;
//	unsigned int id = (unsigned)param2;
//
//	if (id < 0 || id >= rdPtr->layers->size())
//		return false;
//
//	Layer* layer = &(*rdPtr->layers)[id];
//	if (!layer->isValid())
//		return false;
//
//	return ProcessCondition(rdPtr, obj,(long)layer, cndObjOverlapsLayer);
//}

// ============================================================================
//
// ACTIONS
// 
// ============================================================================

ACTION(
	/* ID */			0,
	/* Name */			"Set scroll position to (%0, %1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER,"Scroll X", PARAM_NUMBER,"Scroll Y")
) {
	rdPtr->cameraX = intParam();
	rdPtr->cameraY = intParam();
	rdPtr->rc.rcChanged = true;
}

ACTION(
	/* ID */			1,
	/* Name */			"Set display size to (%0, %1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER,"Display width (pixels)", PARAM_NUMBER,"Display height (pixels)")
) {
	rdPtr->rHo.hoImgWidth = intParam();
	rdPtr->rHo.hoImgWidth = max(0, rdPtr->rHo.hoImgWidth);
	rdPtr->rHo.hoImgHeight = intParam();
	rdPtr->rHo.hoImgHeight = max(0, rdPtr->rHo.hoImgHeight);
	rdPtr->rc.rcChanged = true;
}

ACTION(
	/* ID */			2,
	/* Name */			"Set background color to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_COLOUR,"Background color")
) {
	rdPtr->background = anyParam();
	rdPtr->rc.rcChanged = true;
}

ACTION(
	/* ID */			3,
	/* Name */			"Set tile offset to (%0, %1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER,"X offset (pixels)", PARAM_NUMBER,"Y offset (pixels)")
) {
	rdPtr->callback.offsetX = intParam();
	rdPtr->callback.offsetY = intParam();
}

ACTION(
	/* ID */			4,
	/* Name */			"Enable render callbacks",
	/* Flags */			0,
	/* Params */		(0)
) {
	rdPtr->callback.use = true;
}

ACTION(
	/* ID */			5,
	/* Name */			"Disable render callbacks",
	/* Flags */			0,
	/* Params */		(0)
) {
	rdPtr->callback.use = false;
}


ACTION(
	/* ID */			6,
	/* Name */			"Set tile to (%0, %1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER,"Tileset X", PARAM_NUMBER,"Tileset Y")
) {
	rdPtr->callback.tile.x = (unsigned char)intParam();
	rdPtr->callback.tile.y = (unsigned char)intParam();
}


ACTION(
	/* ID */			7,
	/* Name */			"Set callback tile overflow to (%0, %1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER, "Number of extra tile columns to render on each side (Default: 0)",
							PARAM_NUMBER, "Number of extra tile rows to render on each side (Default: 0)")
) {
	rdPtr->callback.borderX = intParam();
	rdPtr->callback.borderY = intParam();
	rdPtr->callback.borderX = max(0, min(1000, rdPtr->callback.borderX));
	rdPtr->callback.borderY = max(0, min(1000, rdPtr->callback.borderY));
}

ACTION(
	/* ID */			8,
	/* Name */			"Set tile angle to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_NUMBER,"Angle (0-360)")
) {
#ifdef HWABETA
	rdPtr->callback.transform = true;
	rdPtr->callback.angle = fltParam();
#endif
}

ACTION(
	/* ID */			9,
	/* Name */			"Set tile X scale to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_NUMBER,"X scale (1.0 = Default)")
) {
#ifdef HWABETA
	rdPtr->callback.transform = true;
	rdPtr->callback.scaleX = fltParam();
#endif
}

ACTION(
	/* ID */			10,
	/* Name */			"Set tile Y scale to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_NUMBER,"Y scale (1.0 = Default)")
) {
#ifdef HWABETA
	rdPtr->callback.transform = true;
	rdPtr->callback.scaleY = fltParam();
#endif
}

ACTION(
	/* ID */			11,
	/* Name */			"Set tile opacity to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_NUMBER,"Opacity (0-1, 1 = Default)")
) {
	rdPtr->callback.opacity = fltParam();
}

ACTION(
	/* ID */			12,
	/* Name */			"Set tile tint to %0",
	/* Flags */			0,
	/* Params */		(1, PARAM_COLOUR, "Tint (White = Default)")
) {
	rdPtr->callback.tint = anyParam();
}

// ============================================================================
//
// EXPRESSIONS
// 
// ============================================================================


EXPRESSION(
	/* ID */			0,
	/* Name */			"LayerX(",
	/* Flags */			0,
	/* Params */		(1, EXPPARAM_NUMBER, "Layer index")
) {
	unsigned int i = ExParam(TYPE_INT);

	if (i < rdPtr->p->layers->size())
	{
		return getLayerX(rdPtr, &(*rdPtr->p->layers)[i]);
	}
	
	return 0;
}

EXPRESSION(
	/* ID */			1,
	/* Name */			"LayerY(",
	/* Flags */			0,
	/* Params */		(1, EXPPARAM_NUMBER, "Layer index")
) {
	unsigned int i = ExParam(TYPE_INT);

	if (i < rdPtr->p->layers->size())
	{
		return getLayerY(rdPtr, &(*rdPtr->p->layers)[i]);
	}
	
	return 0;
}


EXPRESSION(
	/* ID */			2,
	/* Name */			"CallbackTileX(",
	/* Flags */			0,
	/* Params */		(0)
) {
	return rdPtr->callback.x;
}

EXPRESSION(
	/* ID */			3,
	/* Name */			"CallbackTileY(",
	/* Flags */			0,
	/* Params */		(0)
) {
	return rdPtr->callback.y;
}

EXPRESSION(
	/* ID */			4,
	/* Name */			"CallbackTilesetX(",
	/* Flags */			0,
	/* Params */		(0)
) {
	return rdPtr->callback.tile.x;
}

EXPRESSION(
	/* ID */			5,
	/* Name */			"CallbackTilesetY(",
	/* Flags */			0,
	/* Params */		(0)
) {
	return rdPtr->callback.tile.y;
}