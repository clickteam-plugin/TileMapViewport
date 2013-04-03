// Include guard
#ifndef COMMON_H
#define COMMON_H

// Suppress the deprecated warnings for VC2005
#define _CRT_SECURE_NO_WARNINGS

// General includes
#include	"TemplateInc.h"
#include	<vector>
#include	<fstream>
#include	<shlwapi.h>
#pragma		comment(lib, "shlwapi.lib")
using namespace std;

#define MAXTILESETS	100

#ifdef	HWABETA
#define SURF_DRIVER	SD_D3D9
#define SURF_TYPE	ST_HWA_RTTEXTURE
#else
#define SURF_DRIVER	SD_DIB
#define SURF_TYPE	ST_MEMORY
#endif

#include	"Resource.h"
#include	"FlagsPrefs.h"
#include	"Information.h"
#include	"ImageFlt.h"
#include	"ImgFlt.h"
#include	"CfcFile.h"
#include	"Data.h"
#include	"ObjectSelection.h"
#include	"rTemplate.h"

#include	"ObjectSelection.h"

inline int getLayerX(LPRDATA rdPtr, Layer* layer)
{
	return (int)((layer->offsetX - rdPtr->cameraX) * layer->scrollX);
}

inline int getLayerY(LPRDATA rdPtr, Layer* layer)
{
	return (int)((layer->offsetY - rdPtr->cameraY) * layer->scrollY);
}

inline int floordiv(int x, int d)
{
	if(x >= 0)
		return x/d;

	return -((-x)/d);
}

long ProcessCondition(LPRDATA rdPtr, long param1, long param2, long (*myFunc)(LPRDATA, LPHO, long));
inline float __getFloat(LPRDATA rdPtr) { int foo = CNC_GetFloatParameter(rdPtr); return *(float*)&foo; }

#define intParam() CNC_GetIntParameter(rdPtr)
#define strParam() (const char*)CNC_GetStringParameter(rdPtr)
#define fltParam() __getFloat(rdPtr)
#define objParam() CNC_GetParameter(rdPtr)
#define anyParam() CNC_GetParameter(rdPtr)

// Globals and prototypes
extern HINSTANCE hInstLib;
extern short * conditionsInfos;
extern short * actionsInfos;
extern short * expressionsInfos;
extern long (WINAPI ** ConditionJumps)(LPRDATA rdPtr, long param1, long param2);
extern short (WINAPI ** ActionJumps)(LPRDATA rdPtr, long param1, long param2);
extern long (WINAPI ** ExpressionJumps)(LPRDATA rdPtr, long param);
extern PropData Properties[];
extern WORD DebugTree[];

// End include guard
#endif