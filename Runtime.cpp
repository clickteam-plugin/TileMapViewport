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
	rdPtr->callback.use = false;
	rdPtr->callback.tile = 0;

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
	// We redraw every frame if callbacks are enabled
	if (rdPtr->callback.use)
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
short WINAPI DLLExport DisplayRunObject(LPRDATA rdPtr)
{
	// No attached parent
	if (!rdPtr->p)
		return 0;

	if (rdPtr->autoScroll)
	{
		rdPtr->cameraX = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
		rdPtr->cameraY = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;
	}

	// On-screen coords
	int vX = rdPtr->rHo.hoRect.left;
	int vY = rdPtr->rHo.hoRect.top;
	int width = rdPtr->rHo.hoImgWidth;
	int height = rdPtr->rHo.hoImgHeight;

	// Whether or not we use a temporary surface (-> coordinate shifting)
	bool tempSurf = rdPtr->surface != 0;

	// Whether we have to perform pixel clipping on blitted tiles
	bool clip = !tempSurf && rdPtr->accurateClip;

	// Get output surface
	cSurface* ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	cSurface* target = tempSurf ? rdPtr->surface : ps;
	if (!target)
		return 0;

	// Clear background
	if (!rdPtr->transparent && tempSurf)
		target->Fill(rdPtr->background);

	// For all wanted layers...
	unsigned layerCount = rdPtr->p->layers->size();

	if (!layerCount)
		return 0;

	unsigned minLayer = max(0, min(layerCount - 1, rdPtr->minLayer));
	unsigned maxLayer = max(0, min(layerCount - 1, rdPtr->maxLayer));

	if (minLayer <= maxLayer && maxLayer < layerCount)
	{
		for (unsigned i = minLayer; i <= maxLayer; ++i)
		{
			// Get a pointer to the iterated layer
			Layer* layer = &(*rdPtr->p->layers)[i];

			// No data to draw or simply invisible
			if (!layer->isValid() || !layer->visible)
				continue;
			
			// Store drawing settings for fast access
			unsigned short tileWidth = layer->tileWidth;
			unsigned short tileHeight = layer->tileHeight;

			// Can't render
			if (!tileWidth || !tileHeight)
				continue;

			// Get tileset
			if (layer->tileset >= rdPtr->p->tilesets->size())
				continue;
			Tileset* tileset = &(*rdPtr->p->tilesets)[layer->tileset];	

			// Get the associated tileset image
			cSurface* tileSurf = tileset->texture;
			if (!tileSurf)
				continue;

			// Store the layer size
			int layerWidth = layer->getWidth();
			int layerHeight = layer->getHeight();

			// On-screen coordinate
			int tlX = layer->getScreenX(rdPtr->cameraX);
			int tlY = layer->getScreenY(rdPtr->cameraY);

			// See if the layer is visible at all
			if ((!layer->wrapX && (tlX >= width  || tlX + tileWidth  * layerWidth  < 0))
			||  (!layer->wrapY && (tlY >= height || tlY + tileHeight * layerHeight < 0)))
				continue;

			int x1 = 0;
			int y1 = 0;
			int x2 = layerWidth;
			int y2 = layerHeight;

			// Additional tiles to render outside of the visible area
			int borderX = 1 + (rdPtr->callback.use ? rdPtr->callback.borderX : 0);
			int borderY = 1 + (rdPtr->callback.use ? rdPtr->callback.borderY : 0);

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
			if (layer->wrapX)
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
			if (layer->wrapY)
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
				// Per-tile offset (for callbacks)
				int offsetX = 0, offsetY = 0;

				// For every v	isible tile...
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
						Tile tile = *layer->getTile(tX, tY);

						// Calculate position and render the tile, exit when impossible
						do
						{
							if (tile.id == Tile::EMPTY)
								break;

							// Determine tile opacity
							BlitOp blitOp = layer->opacity < 0.999f ? BOP_BLEND : BOP_COPY;
							float opacity = layer->opacity;
							LPARAM blitParam = 128 - int(opacity * 128);

							// We use callbacks, so let the programmer do stuff
							if (rdPtr->callback.use)
							{
								// We need to map the tile to rdPtr so we can modify its values before rendering
								rdPtr->callback.tile = &tile;

								// Reset some values
								rdPtr->callback.visible = true;
								rdPtr->callback.opacity = 1.0f;
								rdPtr->callback.offsetX = 0;
								rdPtr->callback.offsetY = 0;

								// HWA specific
								rdPtr->callback.tint = WHITE;
								rdPtr->callback.transform = false;
								rdPtr->callback.scaleX = 1.0f;
								rdPtr->callback.scaleY = 1.0f;
								rdPtr->callback.angle = 0.0f;

								// Grant access to tile position
								rdPtr->callback.x = x;
								rdPtr->callback.y = y;

								// Call condition so the programmer can modify the values
								rdPtr->rRd->GenerateEvent(1);

								// Decided not to render tile...
								if (!rdPtr->callback.visible)
									break;

								// Apply offset
								offsetX = rdPtr->callback.offsetX;
								offsetY = rdPtr->callback.offsetY;

								// Apply opacity
								opacity *= rdPtr->callback.opacity;

								// Compute blit operation
								if (rdPtr->callback.tint != WHITE)
								{
									blitOp = BOP_RGBAFILTER;
									int rgb = rdPtr->callback.tint & 0xffffff;
									rgb = ((rgb & 0xff) << 16) | (rgb & 0xff00) | ((rgb & 0xff0000) >> 16);
									blitParam = rgb | (int(opacity * 255) << 24);
								}
								else if (opacity < 0.999)
								{
									blitOp = BOP_BLEND;
									blitParam = 128 - int(opacity * 128);
								}
							}

							// Blit from the surface of the tileset with the tile's index in the layer tilesets
							if (!clip)
							{
								tileSurf->Blit(*target, screenX+offsetX, screenY+offsetY, tile.x * tileWidth, tile.y * tileHeight, tileWidth, tileHeight, BMODE_TRANSP, blitOp, blitParam);
							}
#ifdef HWABETA
							// HWA only: tile angle and scale on callback. Disables accurate clipping!
							else if (rdPtr->callback.transform)
							{
								float scaleX = rdPtr->callback.scaleX;
								float scaleY = rdPtr->callback.scaleY;
								float angle = rdPtr->callback.angle;

								POINT center = {tileWidth/2, tileHeight/2};
								tileSurf->BlitEx(*target, screenX+offsetX+center.x, screenY+offsetY+center.y, scaleX, scaleY, tile.x*tileWidth, tile.y*tileHeight, tileWidth, tileHeight,
									&center, angle, BMODE_TRANSP, blitOp, blitParam);
							}
#endif

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

	// Clear pointer...
	rdPtr->callback.tile = 0;

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