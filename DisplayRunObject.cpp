#include "Common.h"
#include "Helpers.h"

#define assignSubLayerSettingLink(name, minCellSize)                           \
    assignSubLayerLink(layer.settings.subLayerLink, name, minCellSize)

#define assignSubLayerCallbackLink(name, minCellSize)                          \
    assignSubLayerLink(rdPtr->layerCallback.link, name, minCellSize)

#define assignSubLayerLink(source, name, minCellSize)                          \
    {                                                                          \
        if (source.##name != 0xff && source.##name < layer.subLayers.size() && \
            layer.subLayers[source.##name].getCellSize() >= minCellSize) {     \
            sl_##name = &layer.subLayers[source.##name];                       \
        }                                                                      \
    }

// Clips a row of tiles rendered onto a viewport to render only within the
// viewport boundaries
inline void clipBlitTiles(int & destPos, int viewportSize, int & srcPos,
                          int & srcSize, int tileSize)
{
    srcSize = tileSize;

    // Clip left
    if (destPos < 0) {
        srcPos -= destPos;
        srcSize += destPos;
        destPos = 0;
    }

    // Clip right
    if (destPos + srcSize > viewportSize)
        srcSize -= tileSize - (viewportSize - destPos);
}

short WINAPI DLLExport DisplayRunObject(LPRDATA rdPtr)
{
    // No attached parent... nothing to draw...
    if (!rdPtr->p)
        return 0;

    // Whether or not we use a temporary surface (-> coordinate shifting)
    bool tempSurf = rdPtr->surface != 0;

    // Get output surface
    cSurface * ps = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
    cSurface * target = tempSurf ? rdPtr->surface : ps;
    if (!target)
        return 0;

    // On-screen coords
    int viewportX = rdPtr->rHo.hoRect.left;
    int viewportY = rdPtr->rHo.hoRect.top;
    int viewportWidth = rdPtr->rHo.hoImgWidth;
    int viewportHeight = rdPtr->rHo.hoImgHeight;

    // Whether we have to perform pixel clipping on blitted tiles
    bool clip = !tempSurf && rdPtr->accurateClip;

    // Rendering flags for advanced blitting
    DWORD blitFlags = rdPtr->blitFlags;

#ifdef HWABETA
    blitFlags |= BLTF_SAFESRC;
#endif

    // Clear background
    if (!rdPtr->transparent) {
        if (tempSurf)
            target->Fill(rdPtr->background);
        else
            target->Fill(viewportX, viewportY, viewportWidth, viewportHeight,
                         rdPtr->background);
    }

    if (rdPtr->autoScroll) {
        rdPtr->cameraX = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayX;
        rdPtr->cameraY = rdPtr->rHo.hoAdRunHeader->rh3.rh3DisplayY;
    }

    double time = rdPtr->animTime;
    float zoom = rdPtr->zoom;
    float zoomPointX = rdPtr->zoomPointX;
    float zoomPointY = rdPtr->zoomPointY;

    unsigned layerCount = rdPtr->p->layers->size();

    if (!layerCount)
        return 0;

    // Cache all tileset pointers for faster access
    cSurface * tilesetCache[256] = {};
    for (unsigned i = 0; i < rdPtr->p->tilesets->size(); ++i)
        tilesetCache[i] = (*rdPtr->p->tilesets)[i].texture;

    unsigned minLayer = max(0, min(layerCount - 1, rdPtr->minLayer));
    unsigned maxLayer = max(0, min(layerCount - 1, rdPtr->maxLayer));

    if (minLayer <= maxLayer && maxLayer < layerCount) {
        for (unsigned i = minLayer; i <= maxLayer; ++i) {
            // Get a pointer to the iterated layer
            Layer & layer = (*rdPtr->p->layers)[i];

            // Store layer settings locally
            LayerSettings settings = layer.settings;

            // Sub-layer links (assigned via callback)
            SubLayer *sl_tileset = 0, *sl_scaleX = 0, *sl_scaleY = 0,
                     *sl_angle = 0, *sl_animation = 0, *sl_animationFrame = 0;

            // If necessary, load the sub-layers for per-tile render info
            assignSubLayerSettingLink(tileset, 1);
            assignSubLayerSettingLink(animation, 1);
            assignSubLayerSettingLink(animationFrame, 1);

            // Perform layer callback and update sub-layer links if necessary...
            if (rdPtr->layerCallback.use) {
                rdPtr->layerCallback.index = i;
                rdPtr->layerCallback.settings = &settings;

                // Reset layer links
                memset(&rdPtr->layerCallback.link, 0xff,
                       sizeof(rdPtr->layerCallback.link));

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

            // On-screen tile width
            float renderTileWidth = tileWidth * zoom;
            float renderTileHeight = tileHeight * zoom;

#ifdef HWABETA
            POINT tileCenter = {int(renderTileWidth / 2), int(renderTileHeight / 2)};
#endif

            // Can't render
            if (!tileWidth || !tileHeight)
                continue;

            // Store the layer size
            int layerWidth = layer.getWidth();
            int layerHeight = layer.getHeight();

            // On-screen coordinate
            float drawX = layer.getScreenX(rdPtr->cameraX);
            float drawY = layer.getScreenY(rdPtr->cameraY);

            float viewportXBias = viewportWidth * zoomPointX;
            float viewportYBias = viewportHeight * zoomPointY;
            drawX = (drawX - viewportXBias) * zoom + viewportXBias;
            drawY = (drawY - viewportYBias) * zoom + viewportYBias;

            // See if the layer is visible at all
            if ((!settings.wrapX && (drawX >= viewportWidth ||
                                     drawX + renderTileWidth * layerWidth < 0)) ||
                (!settings.wrapY && (drawY >= viewportHeight ||
                                     drawY + renderTileHeight * layerHeight < 0)))
                continue;

            // Region to draw
            int x1 = 0;
            int y1 = 0;
            int x2 = layerWidth; // Exclusive
            int y2 = layerHeight;

            // Additional tiles to render outside of the visible area
            int borderX = 1 + (rdPtr->tileCallback.use ? rdPtr->tileCallback.borderX : 0);
            int borderY = 1 + (rdPtr->tileCallback.use ? rdPtr->tileCallback.borderY : 0);
            int pixelBorderX = int(borderX * renderTileWidth);
            int pixelBorderY = int(borderY * renderTileHeight);

            // Optimize drawing region
            while (drawX <= -pixelBorderX)
                drawX += (int)renderTileWidth, x1++;
            while (drawY <= -pixelBorderY)
                drawY += (int)renderTileHeight, y1++;
            while (drawX >= viewportWidth + pixelBorderX - (x2 - x1) * renderTileWidth)
                x2--;
            while (drawY >= viewportHeight + pixelBorderY - (y2 - y1) * renderTileWidth)
                y2--;

            // Wrapping
            if (settings.wrapX) {
                // TODO: Find optimization
                if (zoom != 1.0f)
                    while (drawX > -renderTileWidth * (borderX - 1))
                        drawX -= renderTileWidth * layerWidth, x1 -= layerWidth;
                else
                    while (drawX > -renderTileWidth * (borderX - 1))
                        drawX -= renderTileWidth, x1++;

                while (drawX < viewportWidth - renderTileWidth * (x2 - x1 - borderX + 1))
                    x2++;

                signmodPair(x1, x2, 0, layerWidth);
            }

            if (settings.wrapY) {
                // TODO: Find optimization
                if (zoom != 1.0f)
                    while (drawY > -renderTileHeight * (borderY - 1))
                        drawY -= renderTileHeight * layerHeight, y1 -= layerHeight;
                else
                    while (drawY > -renderTileHeight * (borderY - 1))
                        drawY -= renderTileHeight, y1++;

                while (drawY < viewportWidth - renderTileWidth * (y2 - y1 - borderY + 1))
                    y2++;

                signmodPair(y1, y2, 0, layerHeight);
            }

            // Draw to screen: Add on-screen offset
            if (!tempSurf) {
                drawX += viewportX;
                drawY += viewportY;
            }

            // If can render
            if (x2 - x1 > 0 && y2 - y1 > 0) {
                // Initialize default render settings of tile
                TileSettings origTileSettings;

                if (sl_scaleX || sl_scaleY || sl_angle)
                    origTileSettings.transform = true;

                // Settings storage that can be modified in a tile callback
                TileSettings tileSettings = origTileSettings;

                // Per-tile offset (for callbacks)
                int offsetX = 0, offsetY = 0;

                // For every visible tile...
                int screenY = (int)drawY;
                for (int unwrappedY = y1; unwrappedY < y2; ++unwrappedY) {
                    int screenX = (int)drawX;
                    for (int unwrappedX = x1; unwrappedX < x2; ++unwrappedX) {
                        // Calculate wrapped coordinate (in case wrapping is
                        // enabled)
                        int x = unwrappedX % layerWidth;
                        int y = unwrappedY % layerHeight;

                        // Get this tile's address
                        Tile tile = *layer.getTile(x, y);

                        // Calculate position and render the tile, exit when
                        // impossible
                        do {
                            if (tile.id == Tile::EMPTY)
                                break;

                            // By default, use the layer tileset
                            unsigned char tilesetIndex = settings.tileset;

                            // Default animation
                            unsigned char animation = 0;
                            unsigned frameOffset = 0;

                            // Sub-layer links
                            if (sl_tileset)
                                tilesetIndex = *sl_tileset->getCell(x, y);
                            if (sl_animation)
                                sl_animation->getCellSafe(x, y, &animation);
                            if (sl_animationFrame)
                                sl_animationFrame->getCellSafe(x, y, &frameOffset);
                            if (sl_scaleX)
                                sl_scaleX->getCellSafe(x, y, &tileSettings.scaleX);
                            if (sl_scaleY)
                                sl_scaleY->getCellSafe(x, y, &tileSettings.scaleY);
                            if (sl_angle)
                                sl_angle->getCellSafe(x, y, &tileSettings.angle);

                            // Determine tile opacity
                            BlitOp blitOp = settings.opacity < 0.999f ? BOP_BLEND : BOP_COPY;
                            float opacity = settings.opacity;
                            LPARAM blitParam = 128 - int(opacity * 128);

                            // We use callbacks, so let the programmer do stuff
                            if (rdPtr->tileCallback.use) {
                                // Reset the tile settings - the last callback
                                // might have changed
                                // them
                                tileSettings = origTileSettings;

                                rdPtr->tileCallback.settings = &tileSettings;
                                tileSettings.tileset = tilesetIndex;

                                // Allow modification of tile values
                                rdPtr->tileCallback.tile = &tile;

                                // Grant access to tile position
                                rdPtr->tileCallback.x = x;
                                rdPtr->tileCallback.y = y;

                                // Call condition so the programmer can modify
                                // the values
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
                                if (tileSettings.tint != WHITE) {
                                    blitOp = BOP_RGBAFILTER;
                                    int rgb = tileSettings.tint & 0xffffff;
                                    rgb = ((rgb & 0xff) << 16) | (rgb & 0xff00) |
                                          ((rgb & 0xff0000) >> 16);
                                    blitParam = rgb | (int(opacity * 255) << 24);
                                }
                                else if (opacity < 0.999) {
                                    blitOp = BOP_BLEND;
                                    blitParam = 128 - int(opacity * 128);
                                }
                            }

                            // Animate if there is more than 1 tile in the
                            // selected animation
                            rdPtr->anim[animation].apply(tile, time, frameOffset);

                            // Get the computed tileset's surface
                            cSurface * tileSurf = tilesetCache[tilesetIndex];

                            if (!tileSurf)
                                break;

#ifdef HWABETA
                            // If zoomed in, act as if tiles were transformed
                            if (zoom != 1.0f)
                                tileSettings.transform = true;

                            // HWA only: tile angle and scale on callback.
                            // Disables accurate
                            // clipping!
                            if (tileSettings.transform) {
                                float scaleX = zoom * tileSettings.scaleX;
                                float scaleY = zoom * tileSettings.scaleY;
                                float angle = tileSettings.angle * 360.0f;

                                float blitX =
                                    drawX +
                                    (screenX - drawX + offsetX + tileCenter.x) * zoom;
                                float blitY =
                                    drawY +
                                    (screenY - drawY + offsetY + tileCenter.y) * zoom;

                                tileSurf->BlitEx(*target, blitX, blitY, scaleX,
                                                 scaleY, tile.x * tileWidth,
                                                 tile.y * tileHeight, tileWidth, tileHeight,
                                                 &tileCenter, angle, BMODE_TRANSP,
                                                 blitOp, blitParam, blitFlags);
                                break;
                            }
#endif

                            // Blit from the surface of the tileset with the
                            // tile's index in the
                            // layer tilesets
                            if (!clip) {
                                tileSurf->Blit(*target, screenX + offsetX, screenY + offsetY,
                                               (tile.x) * tileWidth, tile.y * tileHeight,
                                               tileWidth, tileHeight, BMODE_TRANSP,
                                               blitOp, blitParam, blitFlags);
                            }

                            // Before blitting, perform clipping so that we
                            // won't draw outside of
                            // the viewport...
                            else {

                                tileSurf->Blit(*target, screenX + offsetX, screenY + offsetY,
                                               tile.x * tileWidth, tile.y * tileHeight,
                                               tileWidth, tileHeight, BMODE_TRANSP,
                                               blitOp, blitParam, blitFlags);

                                int dX, dY, sX, sY, sW, sH;
                                dX = screenX + offsetX - viewportX;
                                dY = screenY + offsetY - viewportY;
                                sX = tile.x * tileWidth;
                                sY = tile.y * tileHeight;

                                clipBlitTiles(dX, viewportWidth, sX, sW, tileWidth);
                                clipBlitTiles(dY, viewportHeight, sY, sH, tileHeight);

                                // Draw if still visible
                                if (dX < viewportWidth && dY < viewportHeight &&
                                    sW > 0 && sH > 0) {
                                    // Apply viewport position
                                    dX += viewportX;
                                    dY += viewportY;

                                    tileSurf->Blit(*target, dX, dY, sX, sY, sW, sH, BMODE_TRANSP,
                                                   blitOp, blitParam, blitFlags);
                                }
                            }
                        } while (0);

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
    if (tempSurf) {
        rdPtr->surface->Blit(*ps, viewportX, viewportY, BMODE_OPAQUE,
                             BlitOp(rdPtr->rs.rsEffect & EFFECT_MASK),
                             rdPtr->rs.rsEffectParam, 0);
    }

    // Update window region
    WinAddZone(rdPtr->rHo.hoAdRunHeader->rhIdEditWin, &rdPtr->rHo.hoRect);

    // Clear pointers...
    rdPtr->tileCallback.settings = 0;
    rdPtr->tileCallback.tile = 0;
    rdPtr->layerCallback.settings = 0;

    return 0;
}
