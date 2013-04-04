class rRundata;
typedef rRundata * LPRRDATA;


#include <list>
using namespace std;
#include "Tileset.h"
#include "Tile.h"
#include "Layer.h"

struct RUNDATA;
typedef struct
{
	headerObject	rHo;
	rVal			rv;
	LPRRDATA		rRd;
	int				depth;
	vector<Layer>*	layers;
	vector<Tileset>*tilesets;
	Layer*			currentLayer;
	Tileset*		currentTileset;
	
	unsigned short	tileWidth;
	unsigned short	tileHeight;

	list<RUNDATA*>*	viewports;
	bool			redraw;

	//there's more...
	
} TILEMAP;

struct RUNDATA
{
	//#include "MagicRDATA.h"
	headerObject	rHo;					// Header
	rCom			rc;
	rMvt			rm;
	rSpr			rs;
	rVal			rv;
	LPRRDATA		rRd;

	/* Parent Tile Map */
	TILEMAP*		p;

	/* Depth of the drawing surface */
	/* We use the same depth for all tilesets */
	/* To avoid conversion and ensure maximum speed */
	int				depth;

	/* Scrolling center position */
	int				cameraX;
	int				cameraY;
	bool			autoScroll;

	/* Layer boundaries to draw */
	int				minLayer;
	int				maxLayer;

	/* Misc. settings */
	bool			outsideColl;
	bool			fineColl;

	/* Display surface */
	bool			transparent;
	COLORREF		background;
	cSurface*		surface;
	bool			accurateClip;

	/* Overlaps condition */
	Tileset*		cndTileset;
	cSurface*		cndAlphaSurf;

	/* On collision */
	Tile			collTile;

	/* Callback for rendering */
	struct
	{
		bool		use;

		/* Additional tile render border */
		int			borderX;
		int			borderY;

		/* Data that can be modified */
		bool		visible;
		float		opacity;
		int			offsetX;
		int			offsetY;
		Tile		tile;

		/* HWA specific */
		COLORREF	tint;
		bool		transform;
		float		scaleX;
		float		scaleY;
		float		angle;

		/* Get-only (for now?) */
		int			x;
		int			y;

	}				callback;
	
};
typedef	RUNDATA	* LPRDATA;

// --------------------------------
// EDITION OF OBJECT DATA STRUCTURE
// --------------------------------
// These values let you store data in your extension that will be saved in the MFA
// You should use these with properties

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




typedef struct tagSURFACE
{
headerObject rHo;
rCom rc;
rMvt rm;
rSpr rs;
rVal rv;
LPRRDATA rRd;
//Image bank and important IDs
vector<cSurface*>*          surf;
cSurface*                   target;
short                       targetId;
bool                        targetValid;
cSurface*                   current;
short                       currentId;
short                       lastId;
//Functions
cSurface*                   (*imageAt)(tagSURFACE*,int);
int                         (*imageCount)(tagSURFACE*);
} SURFACE;
