#include "Rundata.h"

typedef TMAPVIEW RUNDATA;
typedef	RUNDATA	* LPRDATA;

typedef struct tagEDATA_V1
{
	extHeader		eHeader;
	short			width;
	short			height;

	bool			autoScroll;

	/* Transparency */
	bool			transparent;
	COLORREF		unused; //transpColor;
	COLORREF		background;

	short			minLayer;
	short			maxLayer;

	bool			outsideColl : 1;
	bool			fineColl : 1;
	bool			accurateClip : 1;
	bool			__boolPadding : 5;

} EDITDATA;
typedef EDITDATA * LPEDATA;