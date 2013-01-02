// ============================================================================
//
// This file contains the actions, conditions and expressions your object uses
// 
// ============================================================================

#include "common.h"

// ============================================================================
//
// CONDITIONS
// 
// ============================================================================

bool cndObjOverlapsLayer(LPRDATA rdPtr, LPRO runObj)
{
	long param = (long)&(*rdPtr->p->layers)[0];
	Layer* layer = (Layer*)param;

	LPHO obj = (LPHO)runObj;
	
	/* Get layer's collision tileset */
	unsigned char tilesetID = (layer->collision != 0xff) ? layer->collision : layer->tileset;
	if(tilesetID >= rdPtr->p->tilesets->size())
		return 0;
	Tileset* tileset = &(*rdPtr->p->tilesets)[tilesetID];

	/* Get tileset's settings */
	cSurface* surface = tileset->surface;
	if(!surface)
		return 0;
	COLORREF transpCol = tileset->transpCol;

	/* Store tile size (we'll need it often) */
	int tileWidth = rdPtr->p->tileWidth;
	int tileHeight = rdPtr->p->tileHeight;

	/* Compute layer position on screen */
	int tlX = int(rdPtr->cameraX*(1 - layer->scrollX) + layer->offsetX * layer->scrollX) + rdPtr->rHo.hoRect.left;
	int tlY = int(rdPtr->cameraY*(1 - layer->scrollY) + layer->offsetY * layer->scrollY) + rdPtr->rHo.hoRect.top;

	/* Get object coordinates */
	int objX = obj->hoX - obj->hoImgXSpot;
	int objY = obj->hoY - obj->hoImgYSpot;

	/* Get layer size in px */
	int layerWidth = layer->width * tileWidth;
	int layerHeight = layer->height * tileHeight;

	/* Not overlapping visible part, exit */
	if(!rdPtr->outsideColl)
	{
		if(objX+obj->hoImgWidth < rdPtr->rHo.hoX
		|| objY+obj->hoImgHeight < rdPtr->rHo.hoY
		|| objX > rdPtr->rHo.hoX+rdPtr->rHo.hoImgWidth
		|| objY > rdPtr->rHo.hoY+rdPtr->rHo.hoImgHeight)
			return 0;
	}

	/* Wrap the coordinates if necessary */
	if(layer->wrapX)
	{
		while(objX < 0)
			objX += layerWidth;


		objX %= layerWidth; 
	}
	if(layer->wrapY)
	{
		while(objY < 0)
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
	x1 = max(0, x1);
	x2 = min(width-1, x2);
	y1 = max(0, y1);
	y2 = min(height-1, y2);

	/* Make object coordinates relative to layer's origin */
	objX1 -= tlX;
	objY1 -= tlY;
	objX2 -= tlX;
	objY2 -= tlY;

	/* Check for any overlapping tile */
	for(int x = x1; x <= x2; ++x)
	{
		for(int y = y1; y <= y2; ++y)
		{
			Tile* tile = layer->data + x + y*width;
			if(tile->x != 0xff && tile->y != 0xff)
			{
				/* Get bounding box of tile */
				int tileX1 = tileWidth * x;
				int tileY1 = tileHeight * y;
				int tileX2 = tileWidth * (x+1);
				int tileY2 = tileHeight * (y+1);

				/* Get intersection box (relative to tile) */
				int x1 = max(objX1, tileX1) - tileX1;
				int y1 = max(objY1, tileY1) - tileY1;
				int x2 = min(objX2, tileX2) - tileX1;
				int y2 = min(objY2, tileY2) - tileY1;

				/* Get position of tile in tileset */
				int tilesetX = tile->x * tileWidth;
				int tilesetY = tile->y * tileHeight;

				bool alpha = surface->HasAlpha() != 0;

				/* Check by alpha channel (TODO: FUCKING OPTIMIZATION) */
				if(alpha)
				{
					cSurface* alphaSurf = surface->GetAlphaSurface();
					BYTE* alphaBuff = alphaSurf->LockBuffer();

					for(int xx = x1; xx < x2; ++xx)
						for(int yy = y1; yy < y2; ++yy)
							if(alphaSurf->GetPixelFast8(tilesetX+xx, tilesetY+yy) > 0)
								return 1;
				
					alphaSurf->UnlockBuffer(alphaBuff);
					surface->ReleaseAlphaSurface(alphaSurf);
				}
				/* Check by transparent color */
				else
				{
					BYTE* buffer = surface->LockBuffer();

					for(int xx = x1; xx < x2; ++xx)
						for(int yy = y1; yy < y2; ++yy)
							if(surface->GetPixelFast(tilesetX+xx, tilesetY+yy) != transpCol)
								return 1;

					surface->UnlockBuffer(buffer);
				}

			}
		}
	}

	return 0;
}

CONDITION(
	/* ID */			0,
	/* Name */			"%o: %0 is overlapping layer %1",
	/* Flags */			EVFLAGS_ALWAYS | EVFLAG2_NOTABLE,
	/* Params */		(2, PARAM_OBJECT,"Object", PARAM_NUMBER,"Layer index")
) {
	printf("%d\n", rdPtr->rHo.hoAdRunHeader->rh4.rh4EventCount);
	if(!rdPtr->p)
		return false;

	unsigned int id = (unsigned)param2;

	if(id >= rdPtr->p->layers->size())
		return false;

	Layer* layer = &(*rdPtr->p->layers)[id];
	if(!layer->isValid())
		return false;

	ObjectSelection select = ObjectSelection(rdPtr->rHo.hoAdRunHeader);

	/* Gets a pointer to the event information structure and find out if the condition is negated */
	PEVT pe = (PEVT)(((LPBYTE)param1)-CND_SIZE);
	bool isNegated = (pe->evtFlags2 & EVFLAG2_NOT);
	short oi = ((eventParam*)param1)->evp.evpW.evpW0;

	return select.FilterObjects(rdPtr, oi, isNegated, cndObjOverlapsLayer);
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
//	if(id < 0 || id >= rdPtr->layers->size())
//		return false;
//
//	Layer* layer = &(*rdPtr->layers)[id];
//	if(!layer->isValid())
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
	/* Name */			"Set callback tile offset to (%0,%1)",
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
	/* Name */			"Set callback tile to (%0,%1)",
	/* Flags */			0,
	/* Params */		(2, PARAM_NUMBER,"Tileset X", PARAM_NUMBER,"Tileset Y")
) {
	rdPtr->callback.tile.x = (unsigned char)intParam();
	rdPtr->callback.tile.y = (unsigned char)intParam();
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

	if(i < rdPtr->p->layers->size())
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

	if(i < rdPtr->p->layers->size())
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
