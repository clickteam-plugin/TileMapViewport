// ===========================================================================
//
// This file contains routines that are handled during the Runtime
// 
// ============================================================================

// Common Include
#include "Common.h"
#include "Helpers.h"

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
	rdPtr->isHwa = rhPtr->rh4.rh4Mv->mvCallFunction(NULL, EF_ISHWA, 0, 0, 0) != 0;
	rdPtr->isUnicode = rhPtr->rh4.rh4Mv->mvCallFunction(NULL, EF_ISUNICODE, 0, 0, 0) != 0;

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

	// HWA blit flags
	rdPtr->blitFlags = edPtr->resample ? STRF_RESAMPLE : 0;

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
	rdPtr->zoomColl = !edPtr->unscaledColl;
	memset(&rdPtr->collMargin, 0, sizeof(RECT));

	// Camera/drawing
	rdPtr->zoom = 1.0f;
	rdPtr->zoomPointX = 0.0f;
	rdPtr->zoomPointY = 0.0f;
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