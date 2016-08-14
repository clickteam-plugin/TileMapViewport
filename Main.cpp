// ============================================================================
//
// This file contains the actions, conditions and expressions your object uses
//
// ============================================================================

#include "Common.h"
#include "Paramacro.h"
#include "Helpers.h"
#include "CollisionFuncs.h"

// ============================================================================
//
// CONDITIONS
//
// ============================================================================

// NOTE: Requires rdPtr->cndTileset to be set accordingly if fineColl is true!
bool cndObjOverlapsLayer(LPRDATA rdPtr, LPRO runObj) {
  Rect rect;
  rect.x1 = runObj->roHo.hoX - runObj->roHo.hoImgXSpot;
  rect.x2 = rect.x1 + runObj->roHo.hoImgWidth;
  rect.y1 = runObj->roHo.hoY - runObj->roHo.hoImgYSpot;
  rect.y2 = rect.y1 + runObj->roHo.hoImgHeight;
  return (long)checkRectangleOverlap(rdPtr, *rdPtr->cndLayer,
                                     *rdPtr->cndTileset, rect);
}

CONDITION(
    /* ID */ 0,
    /* Name */ "%o: %0 is overlapping layer %1",
    /* Flags */ EVFLAGS_ALWAYS | EVFLAGS_NOTABLE,
    /* Params */ (2, PARAM_OBJECT, "Object", PARAM_NUMBER, "Layer index")) {
  LPRO obj = (LPRO)objParam();
  unsigned id = (unsigned)intParam();

  long result = 0;

  if (rdPtr->p && id < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[id];
    if (layer.isValid()) {
      cSurface *surface = prepareLayerCollSurf(rdPtr, layer);

      if (!surface)
        return false;

      // Prepare object selection
      LPRH rhPtr = rdPtr->rHo.hoAdRunHeader;
      ObjectSelection select = ObjectSelection(rdPtr->rHo.hoAdRunHeader,
                                               rdPtr->isHwa, rdPtr->isUnicode);

      // If there's a sub-layer filter, we will cache the required
      // pointers
      cacheOverlapSublayers(rdPtr, layer);

      // Perform overlap test (prepare image buffers for fine collision if
      // necessary)
      BYTE *buff = prepareFineColl(rdPtr, surface);

      // Gets a pointer to the event information structure and find out if
      // the condition is
      // negated
      PEVT pe = (PEVT)(((LPBYTE)param1) - CND_SIZE);
      bool isNegated = (pe->evtFlags2 & EVFLAG2_NOT);
      short oi = ((eventParam *)param1)->evp.evpW.evpW0;

      // Perform test
      rdPtr->cndLayer = &layer;
      result = select.FilterObjects(rdPtr, oi, isNegated, cndObjOverlapsLayer);
      unprepareFineColl(rdPtr, surface, buff);
    }
  }

  // Clear the overlap filter that was populated before this condition was
  // called
  rdPtr->ovlpFilterCount = 0;

  return result;
}

CONDITION(
    /* ID */ 1,
    /* Name */ "%o: On tile",
    /* Flags */ 0,
    /* Params */ (0)) {
  return true;
}

CONDITION(
    /* ID */ 2,
    /* Name */ "%o: Point (%0, %1) is solid on layer %2",
    /* Flags */ EVFLAGS_ALWAYS | EVFLAGS_NOTABLE,
    /* Params */ (3, PARAM_NUMBER, "X coordinate", PARAM_NUMBER, "Y coordinate",
                  PARAM_NUMBER, "Layer index")) {
  int pixelX = intParam();
  int pixelY = intParam();
  unsigned id = intParam();

  if (!rdPtr->p)
    return false;

  if (id >= rdPtr->p->layers->size())
    return false;

  Layer &layer = (*rdPtr->p->layers)[id];
  if (!layer.isValid())
    return false;

  // Get layer's collision tileset
  unsigned char tilesetID = (layer.settings.collision != 0xff)
                                ? layer.settings.collision
                                : layer.settings.tileset;
  if (tilesetID >= rdPtr->p->tilesets->size())
    return false;

  // Required for fine collisions
  Tileset &tileset = (*rdPtr->p->tilesets)[tilesetID];

  return checkPixelSolid(rdPtr, layer, tileset, pixelX, pixelY);
}

CONDITION(
    /* ID */ 3,
    /* Name */ "%o: On layer",
    /* Flags */ 0,
    /* Params */ (0)) {
  return true;
}

CONDITION(
    /* ID */ 4,
    /* Name */ "%o: On layer %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_COMPARAISON, "Layer index")) {
  return rdPtr->layerCallback.index;
}

CONDITION(
    /* ID */ 5,
    /* Name */ "%o | Sub-layer %0 is %1",
    /* Flags */ EVFLAGS_ALWAYS,
    /* Params */ (2, PARAM_NUMBER, "Sub-layer index", PARAM_NUMBER, "Value")) {
  if (rdPtr->p && rdPtr->ovlpFilterCount < 8) {
    OVERLAPFLT &of = rdPtr->ovlpFilters[rdPtr->ovlpFilterCount++];
    of.type = OFT_SUBLAYER;
    of.param = max(0, min(15, param1));
    of.value = param2;
  }
  return true;
}

CONDITION(
    /* ID */ 6,
    /* Name */ "%o | Tileset X is %0",
    /* Flags */ EVFLAGS_ALWAYS,
    /* Params */ (1, PARAM_NUMBER, "Tileset X")) {
  if (rdPtr->p && rdPtr->ovlpFilterCount < 8) {
    OVERLAPFLT &of = rdPtr->ovlpFilters[rdPtr->ovlpFilterCount++];
    of.type = OFT_TILESETX;
    of.value = param1;
  }
  return true;
}

CONDITION(
    /* ID */ 7,
    /* Name */ "%o | Tileset Y is %0",
    /* Flags */ EVFLAGS_ALWAYS,
    /* Params */ (1, PARAM_NUMBER, "Tileset Y")) {
  if (rdPtr->p && rdPtr->ovlpFilterCount < 8) {
    OVERLAPFLT &of = rdPtr->ovlpFilters[rdPtr->ovlpFilterCount++];
    of.type = OFT_TILESETY;
    of.value = param1;
  }
  return true;
}

CONDITION(
    /* ID */ 8,
    /* Name */ "%o: Rect (%0, %1)(%2, %3) is overlapping layer %4",
    /* Flags */ EVFLAGS_ALWAYS | EVFLAGS_NOTABLE,
    /* Params */ (5, PARAM_NUMBER, "Top-left pixel X", PARAM_NUMBER,
                  "Top-left pixel Y", PARAM_NUMBER, "Bottom-right pixel X",
                  PARAM_NUMBER, "Bottom-right pixel Y", PARAM_NUMBER,
                  "Layer index")) {
  Rect rect;
  rect.x1 = intParam();
  rect.y1 = intParam();
  rect.x2 = intParam();
  rect.y2 = intParam();
  unsigned id = (unsigned)intParam();

  long result = 0;

  if (rdPtr->p && id < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[id];
    if (layer.isValid()) {
      cSurface *surface = prepareLayerCollSurf(rdPtr, layer);

      if (!surface)
        return false;

      // If there's a sub-layer filter, we will cache the required
      // pointers
      cacheOverlapSublayers(rdPtr, layer);

      // Perform overlap test (prepare image buffers for fine collision if
      // necessary)
      BYTE *buff = prepareFineColl(rdPtr, surface);
      result = checkRectangleOverlap(rdPtr, layer, *rdPtr->cndTileset, rect);
      unprepareFineColl(rdPtr, surface, buff);
    }
  }

  // Clear the overlap filter that was populated before this condition was
  // called
  rdPtr->ovlpFilterCount = 0;

  return result;
}

CONDITION(
    /* ID */ 9,
    /* Name */ "%o | Tile value is within (%0,%1)(%2,%3)",
    /* Flags */ EVFLAGS_ALWAYS,
    /* Params */ (4, PARAM_NUMBER, "Top-left tileset X", PARAM_NUMBER,
                  "Top-left tileset Y", PARAM_NUMBER, "Bottom-right tileset X",
                  PARAM_NUMBER, "Bottom-right tileset Y")) {
  TileRange range;
  range.a.x = (unsigned char)intParam();
  range.a.y = (unsigned char)intParam();
  range.b.x = (unsigned char)intParam();
  range.b.y = (unsigned char)intParam();
  if (rdPtr->p && rdPtr->ovlpFilterCount < 8) {
    OVERLAPFLT &of = rdPtr->ovlpFilters[rdPtr->ovlpFilterCount++];
    of.type = OFT_TILESETRANGE;
    of.value = *(int *)&range;
  }
  return true;
}

// ============================================================================
//
// ACTIONS
//
// ============================================================================

ACTION(
    /* ID */ 0,
    /* Name */ "Set scroll position to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Scroll X", PARAM_NUMBER, "Scroll Y")) {
  rdPtr->cameraX = intParam();
  rdPtr->cameraY = intParam();
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 1,
    /* Name */ "Set display size to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Display width (pixels)", PARAM_NUMBER,
                  "Display height (pixels)")) {
  rdPtr->rHo.hoImgWidth = intParam();
  rdPtr->rHo.hoImgWidth = max(0, rdPtr->rHo.hoImgWidth);
  rdPtr->rHo.hoImgHeight = intParam();
  rdPtr->rHo.hoImgHeight = max(0, rdPtr->rHo.hoImgHeight);
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 2,
    /* Name */ "Set background color to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_COLOUR, "Background color")) {
  rdPtr->background = anyParam();
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 3,
    /* Name */ "Set tile offset to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "X offset (pixels)", PARAM_NUMBER,
                  "Y offset (pixels)")) {
  if (rdPtr->tileCallback.settings) {
    rdPtr->tileCallback.settings->offsetX = intParam();
    rdPtr->tileCallback.settings->offsetY = intParam();
  }
}

ACTION(
    /* ID */ 4,
    /* Name */ "Enable tile callbacks",
    /* Flags */ 0,
    /* Params */ (0)) {
  rdPtr->tileCallback.use = true;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 5,
    /* Name */ "Disable tile callbacks",
    /* Flags */ 0,
    /* Params */ (0)) {
  rdPtr->tileCallback.use = false;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 6,
    /* Name */ "Set tile to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Tileset X", PARAM_NUMBER, "Tileset Y")) {
  if (rdPtr->tileCallback.tile) {
    rdPtr->tileCallback.tile->x = (unsigned char)intParam();
    rdPtr->tileCallback.tile->y = (unsigned char)intParam();
  }
}

ACTION(
    /* ID */ 7,
    /* Name */ "Set callback tile overflow to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (
        2, PARAM_NUMBER,
        "Number of extra tile columns to render on each side (Default: 0)",
        PARAM_NUMBER,
        "Number of extra tile rows to render on each side (Default: 0)")) {
  rdPtr->tileCallback.borderX = intParam();
  rdPtr->tileCallback.borderY = intParam();
  rdPtr->tileCallback.borderX = max(0, min(1000, rdPtr->tileCallback.borderX));
  rdPtr->tileCallback.borderY = max(0, min(1000, rdPtr->tileCallback.borderY));
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 8,
    /* Name */ "Set tile angle to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Angle (0-1)")) {
#ifdef HWABETA
  if (rdPtr->tileCallback.settings) {
    rdPtr->tileCallback.settings->transform = true;
    rdPtr->tileCallback.settings->angle = fltParam();
  }
#else
// MessageBox(0, "Rotation is only available in HWA.", "Tile Map Error", 0);
#endif
}

ACTION(
    /* ID */ 9,
    /* Name */ "Set tile X scale to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "X scale (1.0 = Default)")) {
#ifdef HWABETA
  if (rdPtr->tileCallback.settings) {
    rdPtr->tileCallback.settings->transform = true;
    rdPtr->tileCallback.settings->scaleX = fltParam();
  }
#else
// MessageBox(0, "Scaling is only available in HWA.", "Tile Map Error", 0);
#endif
}

ACTION(
    /* ID */ 10,
    /* Name */ "Set tile Y scale to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Y scale (1.0 = Default)")) {
#ifdef HWABETA
  if (rdPtr->tileCallback.settings) {
    rdPtr->tileCallback.settings->transform = true;
    rdPtr->tileCallback.settings->scaleY = fltParam();
  }
#else
// MessageBox(0, "Scaling is only available in HWA.", "Tile Map Error", 0);
#endif
}

ACTION(
    /* ID */ 11,
    /* Name */ "Set tile opacity to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Opacity (0-1, 1 = Default)")) {
  if (rdPtr->tileCallback.settings)
    rdPtr->tileCallback.settings->opacity = fltParam();
}

ACTION(
    /* ID */ 12,
    /* Name */ "Set tile tint to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_COLOUR, "Tint (White = Default)")) {
  if (rdPtr->tileCallback.settings)
    rdPtr->tileCallback.settings->tint = anyParam();
}

ACTION(
    /* ID */ 13,
    /* Name */ "Set collision margin to (%0, %1, %2, %3)",
    /* Flags */ 0,
    /* Params */ (4, PARAM_NUMBER, "Extra pixels to the left", PARAM_NUMBER,
                  "Extra pixels to the top", PARAM_NUMBER,
                  "Extra pixels to the right", PARAM_NUMBER,
                  "Extra pixels to the bottom")) {
  rdPtr->collMargin.left = intParam();
  rdPtr->collMargin.top = intParam();
  rdPtr->collMargin.right = intParam();
  rdPtr->collMargin.bottom = intParam();
}

ACTION(
    /* ID */ 14,
    /* Name */ "Set layers to draw to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Minimum layer index", PARAM_NUMBER,
                  "Maximum layer index")) {
  rdPtr->minLayer = intParam();
  rdPtr->maxLayer = intParam();
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 15,
    /* Name */ "Set tile tileset index to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Tileset index (0-255)")) {
  if (rdPtr->tileCallback.settings)
    rdPtr->tileCallback.settings->tileset = (BYTE)intParam();
}

ACTION(
    /* ID */ 16,
    /* Name */ "Set animation timer to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Time in seconds (float)")) {
  float time = fltParam();
  rdPtr->animTime = time;
}

ACTION(
    /* ID */ 17,
    /* Name */ "Advance animation timer by %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Time in seconds (float)")) {
  float time = fltParam();
  rdPtr->animTime += time;
}

ACTION(
    /* ID */ 18,
    /* Name */ "Enable layer callbacks",
    /* Flags */ 0,
    /* Params */ (0)) {
  rdPtr->layerCallback.use = true;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 19,
    /* Name */ "Disable layer callbacks",
    /* Flags */ 0,
    /* Params */ (0)) {
  rdPtr->layerCallback.use = false;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 20,
    /* Name */ "Set layer opacity to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Opacity (0.0-1.0)")) {
  if (rdPtr->layerCallback.settings) {
    float o = fltParam();
    rdPtr->layerCallback.settings->opacity = max(0, min(1, o));
  }
}

ACTION(
    /* ID */ 21,
    /* Name */ "Set layer tileset to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Tileset index (0-255)")) {
  if (rdPtr->layerCallback.settings) {
    rdPtr->layerCallback.settings->tileset = (unsigned char)intParam();
  }
}

ACTION(
    /* ID */ 22,
    /* Name */ "Set layer offset to (%0,%1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Offset X (pixels)", PARAM_NUMBER,
                  "Offset Y (pixels)")) {
  if (rdPtr->layerCallback.settings) {
    rdPtr->layerCallback.settings->offsetX = (short)intParam();
    rdPtr->layerCallback.settings->offsetY = (short)intParam();
  }
}

ACTION(
    /* ID */ 23,
    /* Name */ "Set layer visible to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Visible (0: No, 1: Yes)")) {
  if (rdPtr->layerCallback.settings) {
    rdPtr->layerCallback.settings->visible = intParam() != 0;
  }
}

ACTION(
    /* ID */ 24,
    /* Name */ "Set layer tileset link to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Sub-layer index")) {
  rdPtr->layerCallback.link.tileset = (unsigned char)intParam();
}

ACTION(
    /* ID */ 25,
    /* Name */ "Set layer X scale link to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Sub-layer index")) {
  rdPtr->layerCallback.link.scaleX = (unsigned char)intParam();
}

ACTION(
    /* ID */ 26,
    /* Name */ "Set layer Y scale link to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Sub-layer index")) {
  rdPtr->layerCallback.link.scaleY = (unsigned char)intParam();
}

ACTION(
    /* ID */ 27,
    /* Name */ "Set layer angle link to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Sub-layer index")) {
  rdPtr->layerCallback.link.angle = (unsigned char)intParam();
}

ACTION(
    /* ID */ 28,
    /* Name */ "Set layer animation link to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Sub-layer index")) {
  rdPtr->layerCallback.link.animation = (unsigned char)intParam();
}

ACTION(
    /* ID */ 29,
    /* Name */ "Set animation %0 speed to %1",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Animation index (0-255, 0: Default)",
                  PARAM_NUMBER, "Speed in frames per second")) {
  unsigned char i = (unsigned char)intParam();
  rdPtr->anim[i].speed = fltParam();
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 30,
    /* Name */ "Set animation %0 width to %1",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Animation index (0-255, 0: Default)",
                  PARAM_NUMBER, "Number of tiles to use horizontally")) {
  unsigned char i = (unsigned char)intParam();
  int value = intParam();
  rdPtr->anim[i].width = max(1, value);
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 31,
    /* Name */ "Set animation %0 height to %1",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Animation index (0-255, 0: Default)",
                  PARAM_NUMBER, "Number of tiles to use vertically")) {
  unsigned char i = (unsigned char)intParam();
  int value = intParam();
  rdPtr->anim[i].height = max(1, value);
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 32,
    /* Name */ "Set animation %0 mode to %1",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER, "Animation index (0-255, 0: Default)",
                  PARAM_NUMBER, "Mode (0: Loop, 1: Ping-Pong, 2: Once)")) {
  unsigned char i = (unsigned char)intParam();
  int value = intParam();
  rdPtr->anim[i].mode = (ANIMMODE)value;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 33,
    /* Name */ "Set animation %0 order to %1",
    /* Flags */ 0,
    /* Params */ (
        2, PARAM_NUMBER, "Animation index (0-255, 0: Default)", PARAM_NUMBER,
        "Order (0: Row-major (Rows first), 1: Column-major (Columns first)")) {
  unsigned char i = (unsigned char)intParam();
  int value = intParam();
  rdPtr->anim[i].columnMajor = value != 0;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 34,
    /* Name */ "Set tile animation index to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Animation index (0-255, 0: Default)")) {
  if (rdPtr->tileCallback.settings)
    rdPtr->tileCallback.settings->animation = (unsigned char)intParam();
}

ACTION(
    /* ID */ 35,
    /* Name */ "Set zoom to %0",
    /* Flags */ 0,
    /* Params */ (1, PARAM_NUMBER, "Zoom (1.0: 100%)")) {
  rdPtr->zoom = fltParam();
  rdPtr->zoom = max(0.05f, rdPtr->zoom);
  if (rdPtr->zoom >= 0.999f && rdPtr->zoom <= 1.001f)
    rdPtr->zoom = 1.0f;
  rdPtr->rc.rcChanged = true;
}

ACTION(
    /* ID */ 36,
    /* Name */ "Set zoom origin to (%0, %1)",
    /* Flags */ 0,
    /* Params */ (2, PARAM_NUMBER,
                  "Zoom X origin (0: Left, 0.5: Center, 1: Right)",
                  PARAM_NUMBER,
                  "Zoom Y origin (0: Top, 0.5: Center, 1: Bottom)")) {
  rdPtr->zoomPointX = fltParam();
  rdPtr->zoomPointY = fltParam();
  rdPtr->rc.rcChanged = true;
}

// ============================================================================
//
// EXPRESSIONS
//
// ============================================================================

EXPRESSION(
    /* ID */ 0,
    /* Name */ "LayerViewportX(",
    /* Flags */ 0,
    /* Params */ (1, EXPPARAM_NUMBER, "Layer index")) {
  unsigned i = ExParam(TYPE_INT);

  if (i < rdPtr->p->layers->size()) {
    return (*rdPtr->p->layers)[i].getScreenX(rdPtr->cameraX);
  }

  return 0;
}

EXPRESSION(
    /* ID */ 1,
    /* Name */ "LayerViewportY(",
    /* Flags */ 0,
    /* Params */ (1, EXPPARAM_NUMBER, "Layer index")) {
  unsigned i = ExParam(TYPE_INT);

  if (i < rdPtr->p->layers->size()) {
    return (*rdPtr->p->layers)[i].getScreenY(rdPtr->cameraY);
  }

  return 0;
}

EXPRESSION(
    /* ID */ 2,
    /* Name */ "CallbackTileX(",
    /* Flags */ 0,
    /* Params */ (0)) {
  return rdPtr->tileCallback.x;
}

EXPRESSION(
    /* ID */ 3,
    /* Name */ "CallbackTileY(",
    /* Flags */ 0,
    /* Params */ (0)) {
  return rdPtr->tileCallback.y;
}

EXPRESSION(
    /* ID */ 4,
    /* Name */ "CallbackTilesetX(",
    /* Flags */ 0,
    /* Params */ (0)) {
  return rdPtr->tileCallback.tile ? rdPtr->tileCallback.tile->x : -1;
}

EXPRESSION(
    /* ID */ 5,
    /* Name */ "CallbackTilesetY(",
    /* Flags */ 0,
    /* Params */ (0)) {
  return rdPtr->tileCallback.tile ? rdPtr->tileCallback.tile->y : -1;
}

EXPRESSION(
    /* ID */ 6,
    /* Name */ "Screen2LayerX(",
    /* Flags */ 0,
    /* Params */ (2, EXPPARAM_NUMBER, "Layer index", EXPPARAM_NUMBER,
                  "On-screen X")) {
  unsigned layerIndex = ExParam(TYPE_INT);
  int screen = ExParam(TYPE_INT);

  if (layerIndex < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[layerIndex];
    screen -= rdPtr->rHo.hoX;
    screen +=
        int((rdPtr->cameraX - layer.settings.offsetX) * layer.settings.scrollX);
    screen /= layer.settings.tileWidth * rdPtr->zoom;

    if (layer.settings.wrapX)
      screen = signmod(screen, layer.getWidth());

    return screen;
  }

  return 0;
}

EXPRESSION(
    /* ID */ 7,
    /* Name */ "Screen2LayerY(",
    /* Flags */ 0,
    /* Params */ (2, EXPPARAM_NUMBER, "Layer index", EXPPARAM_NUMBER,
                  "On-screen Y")) {
  unsigned layerIndex = ExParam(TYPE_INT);
  int screen = ExParam(TYPE_INT);

  if (layerIndex < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[layerIndex];
    screen -= rdPtr->rHo.hoY;
    screen +=
        int((rdPtr->cameraY - layer.settings.offsetY) * layer.settings.scrollY);
    screen /= layer.settings.tileWidth * rdPtr->zoom;

    if (layer.settings.wrapY)
      screen = signmod(screen, layer.getHeight());

    return screen;
  }

  return 0;
}

EXPRESSION(
    /* ID */ 8,
    /* Name */ "Layer2ScreenX(",
    /* Flags */ 0,
    /* Params */ (2, EXPPARAM_NUMBER, "Layer index", EXPPARAM_NUMBER,
                  "Tile X")) {
  unsigned i = ExParam(TYPE_INT);
  unsigned pos = ExParam(TYPE_INT);

  if (i < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[i];
    return rdPtr->rHo.hoX + layer.getScreenX(rdPtr->cameraX) +
           layer.settings.tileWidth * pos * rdPtr->zoom;
  }

  return 0;
}

EXPRESSION(
    /* ID */ 9,
    /* Name */ "Layer2ScreenY(",
    /* Flags */ 0,
    /* Params */ (2, EXPPARAM_NUMBER, "Layer index", EXPPARAM_NUMBER,
                  "Tile X")) {
  unsigned i = ExParam(TYPE_INT);
  unsigned pos = ExParam(TYPE_INT);

  if (i < rdPtr->p->layers->size()) {
    Layer &layer = (*rdPtr->p->layers)[i];
    return rdPtr->rHo.hoY + layer.getScreenY(rdPtr->cameraY) +
           layer.settings.tileHeight * pos * rdPtr->zoom;
  }

  return 0;
}

EXPRESSION(
    /* ID */ 10,
    /* Name */ "AnimTimer(",
    /* Flags */ EXPFLAG_DOUBLE,
    /* Params */ (0)) {
  ReturnFloat(rdPtr->animTime);
}

EXPRESSION(
    /* ID */ 11,
    /* Name */ "CallbackLayerIndex(",
    /* Flags */ 0,
    /* Params */ (0)) {
  return rdPtr->layerCallback.index;
}