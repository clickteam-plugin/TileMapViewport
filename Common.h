// Include guard
#ifndef COMMON_H
#define COMMON_H

// Suppress the deprecated warnings for VC2005
#define _CRT_SECURE_NO_WARNINGS

// General includes
#include "TemplateInc.h"
#include <fstream>
#include <shlwapi.h>
#include <vector>
#pragma comment(lib, "shlwapi.lib")
using namespace std;

#include "CfcFile.h"
#include "Data.h"
#include "FlagsPrefs.h"
#include "ImageFlt.h"
#include "ImgFlt.h"
#include "Information.h"
#include "ObjectSelection.h"
#include "Resource.h"
#include "rTemplate.h"

#include "ObjectSelection.h"

#define generateEvent(e) callRunTimeFunction(rdPtr, RFUNCTION_GENERATEEVENT, (e), 0)
bool checkRectangleOverlap(LPRDATA rdPtr, Layer & layer, Tileset & tileset, Rect rect);
bool checkPixelSolid(LPRDATA rdPtr, Layer & layer, Tileset & tileset, int pixelX, int pixelY);

#ifdef HWABETA
#include <d3d9.h>
#endif

// Globals and prototypes
extern HINSTANCE hInstLib;
extern short * conditionsInfos;
extern short * actionsInfos;
extern short * expressionsInfos;
extern long(WINAPI ** ConditionJumps)(LPRDATA rdPtr, long param1, long param2);
extern short(WINAPI ** ActionJumps)(LPRDATA rdPtr, long param1, long param2);
extern long(WINAPI ** ExpressionJumps)(LPRDATA rdPtr, long param);
extern PropData Properties[];
extern WORD DebugTree[];

// End include guard
#endif
