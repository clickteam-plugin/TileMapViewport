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

#endif
	
	LPRH rhPtr = rdPtr->rHo.hoAdRunHeader;

	rdPtr->rHo.hoImgWidth = edPtr->width;
	rdPtr->rHo.hoImgHeight = edPtr->height;
	rdPtr->rc.rcChanged = true;

	/* No parent by default... */
	rdPtr->p = 0;

	/* Create surface, get MMF depth.. */
	cSurface* ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	rdPtr->depth = ps->GetDepth();
	
#ifndef HWABETA
	if(edPtr->transparent)
	{
		rdPtr->surface = 0;
	}
	else
	{
		cSurface *proto;
		GetSurfacePrototype(&proto, rdPtr->depth, SURFACE_TYPE, SURFACE_DRIVER);

		rdPtr->surface = new cSurface;
		rdPtr->surface->Create(rdPtr->rHo.hoImgWidth, rdPtr->rHo.hoImgHeight, proto);
	}
#else
		rdPtr->surface = 0;
#endif

	/* Background settings */
	rdPtr->transparent = edPtr->transparent;
	rdPtr->background = edPtr->background;

	/* Layer stuff */
	rdPtr->minLayer = edPtr->minLayer;
	rdPtr->maxLayer = edPtr->maxLayer;

	/* Misc */
	rdPtr->outsideColl = edPtr->outsideColl;

	/* Camera/drawing */
	rdPtr->cameraX = 0;
	rdPtr->cameraY = 0;
	rdPtr->autoScroll = edPtr->autoScroll;

	/* Initialize callbacks */
	rdPtr->callback.use = false;

	return 0;
}


// ----------------
// DestroyRunObject
// ----------------
// Destroys the run-time object
// 
short WINAPI DLLExport DestroyRunObject(LPRDATA rdPtr, long fast)
{
	/* Detach from tile map */
	if(rdPtr->p)
	{
		rdPtr->p->viewports->remove(rdPtr);
	}

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
	/* We redraw every frame if callbacks are enabled */
	if(rdPtr->callback.use)
		rdPtr->rc.rcChanged = true;

	return rdPtr->rc.rcChanged ? REFLAG_DISPLAY : 0;
}

// ----------------
// DisplayRunObject
// ----------------
// Draw the object in the application screen.
// 
short WINAPI DLLExport DisplayRunObject(LPRDATA rdPtr)
{
	/* No attached parent */
	if(!rdPtr->p)
		return 0;

	if(rdPtr->autoScroll)
	{
		rdPtr->cameraX = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX; // + rdPtr->rHo.hoImgWidth/2;
		rdPtr->cameraY = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY; // + rdPtr->rHo.hoImgHeight/2;
	}
	
	/* Store drawing settings for fast access */
	unsigned short tW = rdPtr->p->tileWidth;
	unsigned short tH = rdPtr->p->tileHeight;

	/* Can't render */
	if(!tW || !tH)
		return 0;

	/* On-screen coords */
	int x = rdPtr->rHo.hoRect.left;
	int y = rdPtr->rHo.hoRect.top;
	int width = rdPtr->rHo.hoImgWidth;
	int height = rdPtr->rHo.hoImgHeight;

	/* Whether or not we use a temporary surface (coordinate shifting) */
#ifdef HWABETA
	bool tempSurf = false;
#else
	bool tempSurf = !rdPtr->transparent;
#endif

	/* Get output surface */
	cSurface* ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	cSurface* target = tempSurf ? rdPtr->surface : ps;

	if(!target)
		return 0;
	
	/* Set on-screen clipping rectangle */
	if(!tempSurf)
		target->SetClipRect(x, y, width, height);
	/* Resize output surface */
	else if(target->GetWidth() != width || target->GetHeight() != height)
		target->Create(width, height, ps);

	/* Clear background */
	if(!rdPtr->transparent)
		target->Fill(rdPtr->background);

	/* For all wanted layers...*/
	int layerCount = rdPtr->p->layers->size();

	if(layerCount == 0)
		return 0;

	int minLayer = max(0, min(layerCount-1, rdPtr->minLayer));
	int maxLayer = max(0, min(layerCount-1, rdPtr->maxLayer));

	if(minLayer <= maxLayer && maxLayer < layerCount)
	{
		for(int i = minLayer; i <= maxLayer; ++i)
		{
			/* Get a pointer to the iterated layer */
			Layer* layer = &(*rdPtr->p->layers)[i];

			/* No data to draw or simply invisible */
			if(!layer->isValid() || !layer->visible)
				continue;

			/* Get tileset */
			if(layer->tileset >= rdPtr->p->tilesets->size())
				continue;
			Tileset* tileset = &(*rdPtr->p->tilesets)[layer->tileset];

			/* Get the associated tileset image */
			cSurface* tileSurf = tileset->surface;
			if(!tileSurf)
				continue;
			
			/* Calculate opacity values */
			BlitOp blitOp = layer->opacity < 0.999 ? BOP_BLEND : BOP_COPY;
			int blitParam = 128 - int(layer->opacity*128.0);

			/* Store the layer size */
			int lW = layer->width;
			int lH = layer->height;

			/* On-screen coordinate */
			int tlX = getLayerX(rdPtr, layer);
			int tlY = getLayerY(rdPtr, layer);

			/* See if the layer is visible at all */
			if((!layer->wrapX && (tlX >= width  || tlX+tW*lW < 0))
			|| (!layer->wrapY && (tlY >= height || tlY+tH*lH < 0)))
				continue;

			int x1 = 0;
			int y1 = 0;
			int x2 = lW;
			int y2 = lH;

			/* Optimize drawing region */
			while(tlX <= -tW)
			{
				tlX += tW;
				x1++;
			}
			while(tlY <= -tH)
			{
				tlY += tH;
				y1++;
			}
			while(tlX + (x2-x1)*tW >= width + tW)
			{
				x2--;
			}
			while(tlY + (y2-y1)*tH >= height + tH)
			{
				y2--;
			}

			/* Wrapping */
			if(layer->wrapX)
			{
				while(tlX > 0)
				{
					tlX -= tW;
					x1--;
				}
				while(tlX + (x2-x1)*tW  < width)
				{
					x2++;
				}
			}
			if(layer->wrapY)
			{
				while(tlY > 0)
				{
					tlY -= tH;
					y1--;
				}
				while(tlY + (y2-y1)*tH < height)
				{
					y2++;
				}
			}

			/* Draw directly to screen, add on-screen offset */
			if(!tempSurf)
			{
				tlX += x;
				tlY += y;
			}

			/* If can render */
			if(x2-x1 > 0 && y2-y1 > 0)
			{
				/* Per-tile offset (for callbacks) */
				int offsetX = 0, offsetY = 0;

				/* For every visible tile... */
				int screenY = tlY;
				for(int y = y1; y < y2; ++y)
				{
					int screenX = tlX;
					for(int x = x1; x < x2; ++x)
					{	
						/* Wrap (if possible anyway) */
						int tX = (x % lW + lW) % lW;
						int tY = (y % lH + lH) % lH;

						/* Get this tile's address */
						Tile* tile = layer->data + tX + tY*lW;

						/* Calculate position and render the tile, exit when impossible */
						do
						{
							/* Only process non-empty tiles */
							if(tile->x == 0xff || tile->y == 0xff)
								break;

							/* We use callbacks, so let the programmer do stuff */
							if(rdPtr->callback.use)
							{
								/* We need to map the tile to rdPtr so we can modify its values before rendering */
								rdPtr->callback.tile = *tile;
								tile = &rdPtr->callback.tile;

								/* Reset some values */
								rdPtr->callback.visible = true;
								rdPtr->callback.offsetX = 0;
								rdPtr->callback.offsetY = 0;

								/* Grant access to tile position */
								rdPtr->callback.x = x;
								rdPtr->callback.y = y;

								/* Call condition so the programmer can modify the values */
								rdPtr->rRd->GenerateEvent(1);

								/* Decided not to render tile... */
								if(!rdPtr->callback.visible)
									break;

								/* Apply offset */
								offsetX = rdPtr->callback.offsetX;
								offsetY = rdPtr->callback.offsetY;
							}

							/* Blit from the surface of the tileset with the tile's index in the layer tilesets */
#ifdef HWABETA
							tileSurf->Blit(*target, screenX+offsetX, screenY+offsetY, /*3.0, 3.0,*/ tile->x*tW, tile->y*tH, tW, tH, /*&center, angle,*/ BMODE_TRANSP, blitOp, blitParam);//, STRF_RESAMPLE);
#else
							tileSurf->Blit(*target, screenX+offsetX, screenY+offsetY, tile->x*tW, tile->y*tH, tW, tH, BMODE_TRANSP, blitOp, blitParam);
#endif
						}
						while(0);

						/* Calculate next on-screen X */
						screenX += tW;
					}

					/* Calculate next on-screen Y */
					screenY += tH;
				}
			}
		}
	}

	/* Finish up */
	if(!tempSurf)
		ps->ClearClipRect();
	else
	{
		rdPtr->surface->Blit(*ps, x, y, BMODE_OPAQUE,
			BlitOp(rdPtr->rs.rsEffect & EFFECT_MASK), rdPtr->rs.rsEffectParam, 0);
	}

	/* Update window region */
	WinAddZone(rdPtr->rHo.hoAdRunHeader->rhIdEditWin, &rdPtr->rHo.hoRect);

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



long ProcessCondition(LPRDATA rdPtr, long param1, long param2, long (*myFunc)(LPRDATA, LPHO, long))
{
	short p1 = ((eventParam*)param1)->evp.evpW.evpW0;
	
	LPRH rhPtr = rdPtr->rHo.hoAdRunHeader;      //get a pointer to the mmf runtime header
	LPOBL objList = rhPtr->rhObjectList;     //get a pointer to the mmf object list
	LPOIL oiList = rhPtr->rhOiList;             //get a pointer to the mmf object info list
	LPQOI qualToOiList = rhPtr->rhQualToOiList; //get a pointer to the mmf qualifier to Oi list
	
	if ( p1 & 0x8000 ) // dealing with a qualifier...
	{
		LPQOI qualToOiStart = (LPQOI)(((char*)qualToOiList) + (p1 & 0x7FFF));
		LPQOI qualToOi = qualToOiStart;
		bool passed = false;
		
		for(qualToOi; qualToOi->qoiOiList >= 0; qualToOi = (LPQOI)(((char*)qualToOi) + 4))
		{
			LPOIL curOi = oiList + qualToOi->qoiOiList;
			
			if(curOi->oilNObjects <= 0) continue;	//No Objects

			bool hasSelection = curOi->oilEventCount == rhPtr->rh2.rh2EventCount;
			if(hasSelection && curOi->oilNumOfSelected <= 0) continue; //No selected objects
			
			LPHO curObj = NULL;
			LPHO prevSelected = NULL;
			int count = 0;
			int selected = 0;
			if(hasSelection) //Already has selected objects
			{
				curObj = objList[curOi->oilListSelected].oblOffset;
				count = curOi->oilNumOfSelected;
			}
			else //No previously selected objects
			{
				curObj = objList[curOi->oilObject].oblOffset;
				count = curOi->oilNObjects;
				curOi->oilEventCount = rhPtr->rh2.rh2EventCount; //tell mmf that the object selection is relevant to this event
			}
			
			for(int i = 0; i < count; i++)
			{
				//Check here
				if(myFunc(rdPtr, curObj, param2))
				{
					if(selected++ == 0)
					{
						curOi->oilListSelected = curObj->hoNumber;
					}
					else
					{
						prevSelected->hoNextSelected = curObj->hoNumber;
					}
					prevSelected = curObj;
				}
				if(hasSelection)
				{
					if(curObj->hoNextSelected >= 0) curObj = objList[curObj->hoNextSelected].oblOffset;
					else break;
				}
				else
				{
					if(curObj->hoNumNext >= 0) curObj = objList[curObj->hoNumNext].oblOffset;
					else break;
				}
			}
			curOi->oilNumOfSelected = selected;
			if ( selected > 0 )
			{
				prevSelected->hoNextSelected = -1;
				passed = true;
			}
			else
			{
				curOi->oilListSelected = -32768;
			}
		}
		
		return passed;
	}
	else	// Not a qualifier
	{
		LPOIL curOi = oiList + p1;
		if(curOi->oilNObjects <= 0) return false;	//No Objects

		bool hasSelection = curOi->oilEventCount == rhPtr->rh2.rh2EventCount;
		if(hasSelection && curOi->oilNumOfSelected <= 0) return false; //No selected objects
		
		LPHO curObj = NULL;
		LPHO prevSelected = NULL;
		int count = 0;
		int selected = 0;
		if(hasSelection) //Already has selected objects
		{
			curObj = objList[curOi->oilListSelected].oblOffset;
			count = curOi->oilNumOfSelected;
		}
		else //No previously selected objects
		{
			curObj = objList[curOi->oilObject].oblOffset;
			count = curOi->oilNObjects;
			curOi->oilEventCount = rhPtr->rh2.rh2EventCount; //tell mmf that the object selection is relevant to this event
		}

		for(int i = 0; i < count; i++)
		{
			//Check here
			if(myFunc(rdPtr, curObj, param2))
			{
				if(selected++ == 0)
				{
					curOi->oilListSelected = curObj->hoNumber;
				}
				else
				{
					prevSelected->hoNextSelected = curObj->hoNumber;
				}
				prevSelected = curObj;
			}
			if(hasSelection)
			{
				if(curObj->hoNextSelected < 0) break;
				else curObj = objList[curObj->hoNextSelected].oblOffset;
			}
			else
			{
				if(curObj->hoNumNext < 0) break;
				else curObj = objList[curObj->hoNumNext].oblOffset;
			}
		}
		curOi->oilNumOfSelected = selected;
		if ( selected > 0 )
		{
			prevSelected->hoNextSelected = -1;
			return true;
		}
		else
		{
			curOi->oilListSelected = -32768;
		}
		return false;
	}
}

