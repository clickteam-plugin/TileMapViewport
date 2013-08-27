// ===========================================================================
//
// This file contains routines that are handled during the Runtime
// 
// ============================================================================

// Common Include
#include	"common.h"

// --------------------
// GetRunObjectDataSize
// --------------------
// Returns the size of the runtime datazone of the object
// 
short WINAPI DLLExport GetRunObjectDataSize(fprh rhPtr, LPEDATA edPtr)
{
	return(sizeof(RUNDATA));
}


// ---------------
// CreateRunObject
// ---------------

short WINAPI DLLExport CreateRunObject(LPRDATA rdPtr, LPEDATA edPtr, fpcob cobPtr)
{
	// Do some rSDK stuff
	#include "rCreateRunObject.h"

#ifdef _DEBUG

	AllocConsole();
	freopen("conin$","r", stdin);
	freopen("conout$","w", stdout);
	freopen("conout$","w", stderr);

	printf("TILEMAPVIEWPORT DEBUG MODE");

#ifdef HWABETA
	printf(" (HWA)");

#endif

	printf("\n");
#endif
	
	LPRH rhPtr = rdPtr->rHo.hoAdRunHeader;

	rdPtr->rHo.hoImgWidth = edPtr->width;
	rdPtr->rHo.hoImgHeight = edPtr->height;
	rdPtr->rc.rcChanged = true;

	// No parent by default...
	rdPtr->p = 0;

	// Create surface, get MMF depth..
	cSurface* ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	rdPtr->depth = ps->GetDepth();

	rdPtr->surface = 0;

#ifndef HWABETA

	/*	Non-transparent surfaces are rendered to an opaque viewport surface so that
		the tiles do not have to be redrawn every single frame.						*/
	if (!edPtr->transparent)
	{
		cSurface* proto;
		GetSurfacePrototype(&proto, rdPtr->depth, ST_MEMORY, SD_DIB);
		rdPtr->surface = new cSurface;
		rdPtr->surface->Create(rdPtr->rHo.hoImgWidth, rdPtr->rHo.hoImgHeight, proto);
	}

#endif

	// Animation settings
	rdPtr->lastTick = GetTickCount();
	rdPtr->animMode = edPtr->animMode;
	for (int i = 0; i < 255; ++i)
	{
		memset(&rdPtr->anim[i], 0, sizeof(Animation));
		rdPtr->anim[i].width = 1;
		rdPtr->anim[i].height = 1;
		rdPtr->anim[i].speed = 8.0f;
	}
	rdPtr->animTime = 0;

	// Background settings
	rdPtr->transparent = edPtr->transparent;
	rdPtr->background = edPtr->background;
	rdPtr->accurateClip = edPtr->accurateClip;

	// Layer stuff
	rdPtr->minLayer = edPtr->minLayer;
	rdPtr->maxLayer = edPtr->maxLayer;

	// Misc
	rdPtr->outsideColl = edPtr->outsideColl;
	rdPtr->fineColl = edPtr->fineColl;
	memset(&rdPtr->collMargin, 0, sizeof(RECT));

	// Camera/drawing
	rdPtr->cameraX = 0;
	rdPtr->cameraY = 0;
	rdPtr->autoScroll = edPtr->autoScroll;

	// Initialize callbacks
	rdPtr->tileCallback.use = false;
	rdPtr->tileCallback.settings = 0;
	rdPtr->tileCallback.tile = 0;
	rdPtr->layerCallback.use = false;
	rdPtr->layerCallback.settings = 0;

	// Overlap stuff
	rdPtr->ovlpFilterCount = 0;

	return 0;
}


// ----------------
// DestroyRunObject
// ----------------
// Destroys the run-time object
// 
short WINAPI DLLExport DestroyRunObject(LPRDATA rdPtr, long fast)
{
	// TM will detach this viewport anyway!

	delete rdPtr->surface;

	// No errors
	delete rdPtr->rRd;
	return 0;
}

// ----------------
// HandleRunObject	
// ----------------
// Called (if you want) each loop, this routine makes the object live
// 
short WINAPI DLLExport HandleRunObject(LPRDATA rdPtr)
{
	// Update animation timer
	if (rdPtr->animMode == 1)
	{
		DWORD tick = GetTickCount();

		// Get elapsed time
		rdPtr->animTime += (tick - rdPtr->lastTick) / 1000.0;

		rdPtr->lastTick = tick;
	}
	else if (rdPtr->animMode == 2)
	{
		// Fixed framerate
		rdPtr->animTime += 1 / (double)rdPtr->rHo.hoAdRunHeader->rhApp->m_hdr.gaFrameRate;
	}

	// We redraw every frame if callbacks are enabled
	if (rdPtr->tileCallback.use || rdPtr->layerCallback.use)
		rdPtr->rc.rcChanged = true;

	// Unlink if deleted
	if (rdPtr->p && rdPtr->p->rHo.hoFlags & HOF_DESTROYED)
		rdPtr->p = 0;

	return rdPtr->rc.rcChanged ? REFLAG_DISPLAY : 0;
}

// ----------------
// DisplayRunObject
// ----------------
// Draw the object in the application screen.
// 

#define assignSubLayerSettingLink(name, minCellSize) \
	assignSubLayerLink(layer.settings.subLayerLink, name, minCellSize)

#define assignSubLayerCallbackLink(name, minCellSize) \
	assignSubLayerLink(rdPtr->layerCallback.link, name, minCellSize)

#define assignSubLayerLink(source, name, minCellSize) \
	{ \
		if (source.##name != 0xff \
		&&	source.##name < layer.subLayers.size() \
		&&	layer.subLayers[source.##name].getCellSize() >= minCellSize) \
		{ sl_##name = &layer.subLayers[source.##name]; } \
	} 1 // Dummy for ';'

short WINAPI DLLExport DisplayRunObject(LPRDATA rdPtr)
{
	// Whether or not we use a temporary surface (-> coordinate shifting)
	bool tempSurf = rdPtr->surface != 0;

	// Whether we have to perform pixel clipping on blitted tiles
	bool clip = !tempSurf && rdPtr->accurateClip;

	// Get output surface
	cSurface* ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	cSurface* target = tempSurf ? rdPtr->surface : ps;
	if (!target)
		return 0;
	
	// On-screen coords
	int vX = rdPtr->rHo.hoRect.left;
	int vY = rdPtr->rHo.hoRect.top;
	int width = rdPtr->rHo.hoImgWidth;
	int height = rdPtr->rHo.hoImgHeight;

	// Clear background
	if (!rdPtr->transparent)
	{
		if (tempSurf)
			target->Fill(rdPtr->background);
		else
			target->Fill(vX, vY, width, height, rdPtr->background);
	}

	// No attached parent... nothing to draw...
	if (!rdPtr->p)
		return 0;

	if (rdPtr->autoScroll)
	{
		rdPtr->cameraX = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
		rdPtr->cameraY = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;
	}

	double time = rdPtr->animTime;


	// For all wanted layers...
	unsigned layerCount = rdPtr->p->layers->size();

	if (!layerCount)
		return 0;

	// Cache all tileset pointers for faster access
	cSurface* tilesetCache[256] = {};
	for (unsigned i = 0; i < rdPtr->p->tilesets->size(); ++i)
		tilesetCache[i] = (*rdPtr->p->tilesets)[i].texture;

	unsigned minLayer = max(0, min(layerCount - 1, rdPtr->minLayer));
	unsigned maxLayer = max(0, min(layerCount - 1, rdPtr->maxLayer));

	if (minLayer <= maxLayer && maxLayer < layerCount)
	{
		for (unsigned i = minLayer; i <= maxLayer; ++i)
		{
			// Get a pointer to the iterated layer
			Layer& layer = (*rdPtr->p->layers)[i];

			// Store layer settings locally
			LayerSettings settings = layer.settings;

			// Sub-layer links (assigned via callback)
			SubLayer* sl_tileset = 0, *sl_scaleX = 0, *sl_scaleY = 0, *sl_angle = 0, *sl_animation = 0;

			// If necessary, load the sub-layers for per-tile render info
			assignSubLayerSettingLink(tileset, 1);
			assignSubLayerSettingLink(animation, 1);

			// Perform layer callback and update sub-layer links if necessary...
			if (rdPtr->layerCallback.use)
			{
				rdPtr->layerCallback.index = i;
				rdPtr->layerCallback.settings = &settings;

				// Reset layer links
				memset(&rdPtr->layerCallback.link, 0xff, sizeof(rdPtr->layerCallback.link));

				// Perform callback
				generateEvent(3);
				generateEvent(4);

				// Assign linked sub-layer pointers
				assignSubLayerCallbackLink(tileset, 1);
				assignSubLayerCallbackLink(animation, 1);
				assignSubLayerCallbackLink(scaleX, 4);
				assignSubLayerCallbackLink(scaleY, 4);
				assignSubLayerCallbackLink(angle, 4);

			}

			// No data to draw or simply invisible
			if (!layer.isValid() || !settings.visible)
				continue;
			
			// We'll need these so often...
			unsigned short tileWidth = settings.tileWidth;
			unsigned short tileHeight = settings.tileHeight;

			// Can't render
			if (!tileWidth || !tileHeight)
				continue;

			// Store the layer size
			int layerWidth = layer.getWidth();
			int layerHeight = layer.getHeight();

			// On-screen coordinate
			int tlX = layer.getScreenX(rdPtr->cameraX);
			int tlY = layer.getScreenY(rdPtr->cameraY);

			// See if the layer is visible at all
			if ((!settings.wrapX && (tlX >= width  || tlX + tileWidth  * layerWidth  < 0))
			||  (!settings.wrapY && (tlY >= height || tlY + tileHeight * layerHeight < 0)))
				continue;

			int x1 = 0;
			int y1 = 0;
			int x2 = layerWidth;
			int y2 = layerHeight;

			// Additional tiles to render outside of the visible area
			int borderX = 1 + (rdPtr->tileCallback.use ? rdPtr->tileCallback.borderX : 0);
			int borderY = 1 + (rdPtr->tileCallback.use ? rdPtr->tileCallback.borderY : 0);

			// Optimize drawing region
			while (tlX <= -tileWidth*borderX)
				tlX += tileWidth, x1++;
			while (tlY <= -tileHeight*borderY)
				tlY += tileHeight, y1++;
			while (tlX + (x2-x1)*tileWidth >= width + tileWidth*borderX)
				x2--;
			while (tlY + (y2-y1)*tileHeight >= height + tileHeight*borderY)
				y2--;

			// Wrapping
			if (settings.wrapX)
			{
				while (tlX > -tileWidth*(borderX-1))
				{
					tlX -= tileWidth;
					x1--;
				}
				while (tlX + (x2-x1-borderX+1)*tileWidth  < width)
				{
					x2++;
				}
			}
			if (settings.wrapY)
			{
				while (tlY > -tileHeight*(borderY-1))
				{
					tlY -= tileHeight;
					y1--;
				}
				while (tlY + (y2-y1-borderY+1)*tileHeight < height)
				{
					y2++;
				}
			}

			// Draw to screen: Add on-screen offset
			int onScreenX = tlX + (tempSurf ? 0 : vX);
			int onScreenY = tlY + (tempSurf ? 0 : vY);

			// If can render
			if (x2-x1 > 0 && y2-y1 > 0)
			{
				// Initialize default render settings of tile
				TileSettings origTileSettings;

				if (sl_scaleX || sl_scaleY || sl_angle)
					origTileSettings.transform = true;

				// Settings storage that can be modified in a tile callback
				TileSettings tileSettings = origTileSettings;

				// Per-tile offset (for callbacks)
				int offsetX = 0, offsetY = 0;

				// For every visible tile...
				int screenY = onScreenY;
				for (int y = y1; y < y2; ++y)
				{
					int screenX = onScreenX;
					for (int x = x1; x < x2; ++x)
					{
						// Wrap (if possible anyway)
						int tX = (x % layerWidth + layerWidth) % layerWidth;
						int tY = (y % layerHeight + layerHeight) % layerHeight;

						// Get this tile's address
						Tile tile = *layer.getTile(tX, tY);

						// Calculate position and render the tile, exit when impossible
						do
						{
							if (tile.id == Tile::EMPTY)
								break;

							// By default, use the layer tileset
							unsigned char tilesetIndex = settings.tileset;

							// Default animation
							unsigned char animation = 0;

							// Sub-layer links
							if (sl_tileset)
								tilesetIndex = *sl_tileset->getCell(tX, tY);
							if (sl_animation)
								sl_animation->getCellSafe(tX, tY, &animation);
							if (sl_scaleX)
								sl_scaleX->getCellSafe(tX, tY, &tileSettings.scaleX);
							if (sl_scaleY)
								sl_scaleY->getCellSafe(tX, tY, &tileSettings.scaleY);
							if (sl_angle)
								sl_angle->getCellSafe(tX, tY, &tileSettings.angle);

							// Determine tile opacity
							BlitOp blitOp = settings.opacity < 0.999f ? BOP_BLEND : BOP_COPY;
							float opacity = settings.opacity;
							LPARAM blitParam = 128 - int(opacity * 128);

							// We use callbacks, so let the programmer do stuff
							if (rdPtr->tileCallback.use)
							{
								// Reset the tile settings - the last callback might have changed them
								tileSettings = origTileSettings;

								rdPtr->tileCallback.settings = &tileSettings;
								tileSettings.tileset = tilesetIndex;

								// Allow modification of tile values
								rdPtr->tileCallback.tile = &tile;

								// Grant access to tile position
								rdPtr->tileCallback.x = x;
								rdPtr->tileCallback.y = y;

								// Call condition so the programmer can modify the values
								generateEvent(1);

								// Decided not to render tile...
								if (!tileSettings.visible)
									break;

								// Apply tileset index
								tilesetIndex = tileSettings.tileset;

								// Apply offset
								offsetX = tileSettings.offsetX;
								offsetY = tileSettings.offsetY;

								// Apply opacity
								opacity *= tileSettings.opacity;

								// Compute blit operation
								if (tileSettings.tint != WHITE)
								{
									blitOp = BOP_RGBAFILTER;
									int rgb = tileSettings.tint & 0xffffff;
									rgb = ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb & 0xff0000) >> 16);
									blitParam = rgb | (int(opacity * 255) << 24);
								}
								else if (opacity < 0.999)
								{
									blitOp = BOP_BLEND;
									blitParam = 128 - int(opacity * 128);
								}
							}

							// Animate if there is more than 1 tile in the selected animation
							Animation& a = rdPtr->anim[animation];
							const int tileCount = a.width * a.height;
							if (tileCount > 1)
							{
								int tileIndex = int(time * a.speed);
								while (tileIndex < 0)
									tileIndex += tileCount;

								// _ - ^ _ - ^ ...
								if(a.mode == AM_LOOP)
									tileIndex %= tileCount;

								// _ - ^ - _ - ...
								else if (a.mode == AM_PINGPONG)
								{
									tileIndex %= 2 * tileCount - 2;
									if (tileIndex >= tileCount)
										tileIndex = 2 * tileCount - tileIndex - 2;
								}
								// _ - ^ ^ ^ ...
								else if (a.mode == AM_ONCE)
								{
									tileIndex = min(tileCount - 1, tileIndex);
								}

								// Y first, then X
								if (a.columnMajor)
								{
									tile.x += tileIndex / a.height;
									tile.y += tileIndex % a.height;
								}
								// X first, then Y
								else
								{
									tile.x += tileIndex % a.width;
									tile.y += tileIndex / a.width;
								}
							}

							// Get the computed tileset's surface
							cSurface* tileSurf = tilesetCache[tilesetIndex];

							if (!tileSurf)
								break;

#ifdef HWABETA
							// HWA only: tile angle and scale on callback. Disables accurate clipping!
							if (tileSettings.transform)
							{
								float scaleX = tileSettings.scaleX;
								float scaleY = tileSettings.scaleY;
								float angle = tileSettings.angle * 360.0f;

								POINT center = {tileWidth/2, tileHeight/2};
								tileSurf->BlitEx(*target, screenX+offsetX+center.x, screenY+offsetY+center.y, scaleX, scaleY, tile.x*tileWidth, tile.y*tileHeight, tileWidth, tileHeight,
									&center, angle, BMODE_TRANSP, blitOp, blitParam);
								break;
							}
#endif
							
							// Blit from the surface of the tileset with the tile's index in the layer tilesets
							if (!clip)
							{
								tileSurf->Blit(*target, screenX+offsetX, screenY+offsetY, tile.x * tileWidth, tile.y * tileHeight, tileWidth, tileHeight, BMODE_TRANSP, blitOp, blitParam);
							}

							// Before blitting, perform clipping so that we won't draw outside of the viewport...
							else
							{
								int dX, dY, sX, sY, sW, sH;
								dX = screenX + offsetX - vX;
								dY = screenY + offsetY - vY;
								sX = tile.x * tileWidth;
								sY = tile.y * tileHeight;
								sW = tileWidth;
								sH = tileHeight;

								// Clip left
								if (dX < 0)
								{
									sX -= dX;
									sW += dX;
									dX = 0;
								}

								// Clip top
								if (dY < 0)
								{
									sY -= dY;
									sH += dY;
									dY = 0;
								}
								
								// Clip right
								if (dX + sW > width)
									sW -= tileWidth - (width-dX);

								// Clip bottom
								if (dY + sH > height)
									sH -= tileHeight - (height-dY);

								if (dX < width && dY < height && sW > 0 && sH > 0)
								{
									// Apply viewport position
									dX += vX;
									dY += vY;

									tileSurf->Blit(*target, dX, dY, sX, sY, sW, sH, BMODE_TRANSP, blitOp, blitParam);
								}
							}
						}
						while (0);

						// Calculate next on-screen X
						screenX += tileWidth;
					}

					// Calculate next on-screen Y
					screenY += tileHeight;
				}
			}
		}
	}

	// Finish up
	if (tempSurf)
	{
		rdPtr->surface->Blit(*ps, vX, vY, BMODE_OPAQUE,
			BlitOp(rdPtr->rs.rsEffect & EFFECT_MASK), rdPtr->rs.rsEffectParam, 0);
	}

	// Update window region
	WinAddZone(rdPtr->rHo.hoAdRunHeader->rhIdEditWin, &rdPtr->rHo.hoRect);

	// Clear pointers...
	rdPtr->tileCallback.settings = 0;
	rdPtr->tileCallback.tile = 0;
	rdPtr->layerCallback.settings = 0;

	return 0;
}

// -------------------
// GetRunObjectSurface
// -------------------
// Implement this function instead of DisplayRunObject if your extension
// supports ink effects and transitions. Note: you can support ink effects
// in DisplayRunObject too, but this is automatically done if you implement
// GetRunObjectSurface (MMF applies the ink effect to the transition).
//
// Note: do not forget to enable the function in the .def file
// if you remove the comments below.

//cSurface* WINAPI DLLExport GetRunObjectSurface(LPRDATA rdPtr)
//{
//	return 0;
//}


// -------------------------
// GetRunObjectCollisionMask
// -------------------------
// Implement this function if your extension supports fine collision mode (OEPREFS_FINECOLLISIONS),
// Or if it's a background object and you want Obstacle properties for this object.
//
// Should return NULL if the object is not transparent.
//
// Note: do not forget to enable the function in the .def file
// if you remove the comments below.
//
/*
cSurface* WINAPI DLLExport GetRunObjectCollisionMask(LPRDATA rdPtr, LPARAM lParam)
{
	// Typical example for active objects
	// ----------------------------------
	// Opaque? collide with box
	if ( (rdPtr->rs.rsEffect & EFFECTFLAG_TRANSPARENT) == 0 )	// Note: only if your object has the OEPREFS_INKEFFECTS option
		return NULL;

	// Transparent? Create mask
	LPSMASK pMask = rdPtr->m_pColMask;
	if ( pMask == NULL )
	{
		if ( rdPtr->m_pSurface != NULL )
		{
			DWORD dwMaskSize = rdPtr->m_pSurface->CreateMask(NULL, lParam);
			if ( dwMaskSize != 0 )
			{
				pMask = (LPSMASK)calloc(dwMaskSize, 1);
				if ( pMask != NULL )
				{
					rdPtr->m_pSurface->CreateMask(pMask, lParam);
					rdPtr->m_pColMask = pMask;
				}
			}
		}
	}

	// Note: for active objects, lParam is always the same.
	// For background objects (OEFLAG_BACKGROUND), lParam maybe be different if the user uses your object
	// as obstacle and as platform. In this case, you should store 2 collision masks
	// in your data: one if lParam is 0 and another one if lParam is different from 0.

	return pMask;
}
*/

// ----------------
// PauseRunObject
// ----------------
// Enters the pause mode
// 
short WINAPI DLLExport PauseRunObject(LPRDATA rdPtr)
{
	// Ok
	return 0;
}


// -----------------
// ContinueRunObject
// -----------------
// Quits the pause mode
//
short WINAPI DLLExport ContinueRunObject(LPRDATA rdPtr)
{
	// Ok
	return 0;
}


// ============================================================================
//
// START APP / END APP / START FRAME / END FRAME routines
// 
// ============================================================================

// -------------------
// StartApp
// -------------------
// Called when the application starts or restarts.
// Useful for storing global data
// 
void WINAPI DLLExport StartApp(mv _far *mV, CRunApp* pApp)
{
	// Example
	// -------
	// Delete global data (if restarts application)
//	CMyData* pData = (CMyData*)mV->mvGetExtUserData(pApp, hInstLib);
//	if ( pData != NULL )
//	{
//		delete pData;
//		mV->mvSetExtUserData(pApp, hInstLib, NULL);
//	}
}

// -------------------
// EndApp
// -------------------
// Called when the application ends.
// 
void WINAPI DLLExport EndApp(mv _far *mV, CRunApp* pApp)
{
	// Example
	// -------
	// Delete global data
//	CMyData* pData = (CMyData*)mV->mvGetExtUserData(pApp, hInstLib);
//	if ( pData != NULL )
//	{
//		delete pData;
//		mV->mvSetExtUserData(pApp, hInstLib, NULL);
//	}
}

// -------------------
// StartFrame
// -------------------
// Called when the frame starts or restarts.
// 
void WINAPI DLLExport StartFrame(mv _far *mV, DWORD dwReserved, int nFrameIndex)
{

}

// -------------------
// EndFrame
// -------------------
// Called when the frame ends.
// 
void WINAPI DLLExport EndFrame(mv _far *mV, DWORD dwReserved, int nFrameIndex)
{

}

// ============================================================================
//
// TEXT ROUTINES (if OEFLAG_TEXT)
// 
// ============================================================================

// -------------------
// GetRunObjectFont
// -------------------
// Return the font used by the object.
// 
/*

  // Note: do not forget to enable the functions in the .def file
  // if you remove the comments below.

void WINAPI GetRunObjectFont(LPRDATA rdPtr, LOGFONT* pLf)
{
	// Example
	// -------
	// GetObject(rdPtr->m_hFont, sizeof(LOGFONT), pLf);
}

// -------------------
// SetRunObjectFont
// -------------------
// Change the font used by the object.
// 
void WINAPI SetRunObjectFont(LPRDATA rdPtr, LOGFONT* pLf, RECT* pRc)
{
	// Example
	// -------
//	HFONT hFont = CreateFontIndirect(pLf);
//	if ( hFont != NULL )
//	{
//		if (rdPtr->m_hFont!=0)
//			DeleteObject(rdPtr->m_hFont);
//		rdPtr->m_hFont = hFont;
//		SendMessage(rdPtr->m_hWnd, WM_SETFONT, (WPARAM)rdPtr->m_hFont, FALSE);
//	}

}

// ---------------------
// GetRunObjectTextColor
// ---------------------
// Return the text color of the object.
// 
COLORREF WINAPI GetRunObjectTextColor(LPRDATA rdPtr)
{
	// Example
	// -------
	return 0;	// rdPtr->m_dwColor;
}

// ---------------------
// SetRunObjectTextColor
// ---------------------
// Change the text color of the object.
// 
void WINAPI SetRunObjectTextColor(LPRDATA rdPtr, COLORREF rgb)
{
	// Example
	// -------
	rdPtr->m_dwColor = rgb;
	InvalidateRect(rdPtr->m_hWnd, NULL, TRUE);
}
*/


// ============================================================================
//
// DEBUGGER ROUTINES
// 
// ============================================================================

// -----------------
// GetDebugTree
// -----------------
// This routine returns the address of the debugger tree
//
LPWORD WINAPI DLLExport GetDebugTree(LPRDATA rdPtr)
{
#if !defined(RUN_ONLY)
	return DebugTree;
#else
	return NULL;
#endif // !defined(RUN_ONLY)
}

// -----------------
// GetDebugItem
// -----------------
// This routine returns the text of a given item.
//
void WINAPI DLLExport GetDebugItem(LPSTR pBuffer, LPRDATA rdPtr, int id)
{
#if !defined(RUN_ONLY)

	// Example
	// -------
/*
	char temp[DB_BUFFERSIZE];

	switch (id)
	{
	case DB_CURRENTSTRING:
		LoadString(hInstLib, IDS_CURRENTSTRING, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->text);
		break;
	case DB_CURRENTVALUE:
		LoadString(hInstLib, IDS_CURRENTVALUE, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->value);
		break;
	case DB_CURRENTCHECK:
		LoadString(hInstLib, IDS_CURRENTCHECK, temp, DB_BUFFERSIZE);
		if (rdPtr->check)
			wsprintf(pBuffer, temp, "TRUE");
		else
			wsprintf(pBuffer, temp, "FALSE");
		break;
	case DB_CURRENTCOMBO:
		LoadString(hInstLib, IDS_CURRENTCOMBO, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->combo);
		break;
	}
*/

#endif // !defined(RUN_ONLY)
}

// -----------------
// EditDebugItem
// -----------------
// This routine allows to edit editable items.
//
void WINAPI DLLExport EditDebugItem(LPRDATA rdPtr, int id)
{
#if !defined(RUN_ONLY)

	// Example
	// -------
/*
	switch (id)
	{
	case DB_CURRENTSTRING:
		{
			EditDebugInfo dbi;
			char buffer[256];

			dbi.pText=buffer;
			dbi.lText=TEXT_MAX;
			dbi.pTitle=NULL;

			strcpy(buffer, rdPtr->text);
			long ret=callRunTimeFunction(rdPtr, RFUNCTION_EDITTEXT, 0, (LPARAM)&dbi);
			if (ret)
				strcpy(rdPtr->text, dbi.pText);
		}
		break;
	case DB_CURRENTVALUE:
		{
			EditDebugInfo dbi;

			dbi.value=rdPtr->value;
			dbi.pTitle=NULL;

			long ret=callRunTimeFunction(rdPtr, RFUNCTION_EDITINT, 0, (LPARAM)&dbi);
			if (ret)
				rdPtr->value=dbi.value;
		}
		break;
	}
*/
#endif // !defined(RUN_ONLY)
}