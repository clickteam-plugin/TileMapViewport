#include "Common.h"
#include "Helpers.h"

/*
 * Animation
 */

void Animation::apply(Tile & tile, double time, int frameOffset)
{
    const int tileCount = width * height;
    if (tileCount > 1) {
        int tileIndex = int(time * speed) + frameOffset;

        // _ - ^ _ - ^ ...
        if (mode == AM_LOOP)
            tileIndex = signmod(tileIndex, tileCount);

        // _ - ^ - _ - ...
        else if (mode == AM_PINGPONG) {
            tileIndex = signmod(tileIndex, 2 * tileCount - 2);
            if (tileIndex >= tileCount)
                tileIndex = 2 * tileCount - tileIndex - 2;
        }

        // _ - ^ ^ ^ ...
        else if (mode == AM_ONCE) {
            tileIndex = min(tileCount - 1, tileIndex);
        }

        // Y first, then X
        if (columnMajor) {
            tile.x += tileIndex / height;
            tile.y += tileIndex % height;
        }
        // X first, then Y
        else {
            tile.x += tileIndex % width;
            tile.y += tileIndex / width;
        }
    }
}
