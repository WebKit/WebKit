// Copyright 2022 Google LLC
// SPDX-License-Identifier: BSD-2-Clause

#include "avifexif.h"
#include "avif/avif.h"

uint8_t avifImageGetExifOrientationFromIrotImir(const avifImage * image)
{
    if ((image->transformFlags & AVIF_TRANSFORM_IROT) && (image->irot.angle == 1)) {
        if (image->transformFlags & AVIF_TRANSFORM_IMIR) {
            if (image->imir.axis) {
                return 7; // 90 degrees anti-clockwise then swap left and right.
            }
            return 5; // 90 degrees anti-clockwise then swap top and bottom.
        }
        return 6; // 90 degrees anti-clockwise.
    }
    if ((image->transformFlags & AVIF_TRANSFORM_IROT) && (image->irot.angle == 2)) {
        if (image->transformFlags & AVIF_TRANSFORM_IMIR) {
            if (image->imir.axis) {
                return 4; // 180 degrees anti-clockwise then swap left and right.
            }
            return 2; // 180 degrees anti-clockwise then swap top and bottom.
        }
        return 3; // 180 degrees anti-clockwise.
    }
    if ((image->transformFlags & AVIF_TRANSFORM_IROT) && (image->irot.angle == 3)) {
        if (image->transformFlags & AVIF_TRANSFORM_IMIR) {
            if (image->imir.axis) {
                return 5; // 270 degrees anti-clockwise then swap left and right.
            }
            return 7; // 270 degrees anti-clockwise then swap top and bottom.
        }
        return 8; // 270 degrees anti-clockwise.
    }
    if (image->transformFlags & AVIF_TRANSFORM_IMIR) {
        if (image->imir.axis) {
            return 2; // Swap left and right.
        }
        return 4; // Swap top and bottom.
    }
    return 1; // Default orientation ("top-left", no-op).
}

avifResult avifSetExifOrientation(avifRWData * exif, uint8_t orientation)
{
    size_t offset;
    const avifResult result = avifGetExifOrientationOffset(exif->data, exif->size, &offset);
    if (result != AVIF_RESULT_OK) {
        return result;
    }
    if (offset < exif->size) {
        exif->data[offset] = orientation;
        return AVIF_RESULT_OK;
    }
    // No Exif orientation was found.
    if (orientation == 1) {
        // The default orientation is 1, so if the given orientation is 1 too, do nothing.
        return AVIF_RESULT_OK;
    }
    // Adding an orientation tag to an Exif payload is involved.
    return AVIF_RESULT_NOT_IMPLEMENTED;
}
