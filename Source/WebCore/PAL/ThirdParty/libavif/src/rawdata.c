// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <string.h>

avifResult avifRWDataRealloc(avifRWData * raw, size_t newSize)
{
    if (raw->size != newSize) {
        uint8_t * newData = avifAlloc(newSize);
        AVIF_CHECKERR(newData, AVIF_RESULT_OUT_OF_MEMORY);
        if (raw->size && newSize) {
            memcpy(newData, raw->data, AVIF_MIN(raw->size, newSize));
        }
        avifFree(raw->data);
        raw->data = newData;
        raw->size = newSize;
    }
    return AVIF_RESULT_OK;
}

avifResult avifRWDataSet(avifRWData * raw, const uint8_t * data, size_t len)
{
    if (len) {
        AVIF_CHECKRES(avifRWDataRealloc(raw, len));
        memcpy(raw->data, data, len);
    } else {
        avifRWDataFree(raw);
    }
    return AVIF_RESULT_OK;
}

void avifRWDataFree(avifRWData * raw)
{
    avifFree(raw->data);
    raw->data = NULL;
    raw->size = 0;
}
