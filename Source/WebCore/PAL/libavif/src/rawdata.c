// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <string.h>

void avifRWDataRealloc(avifRWData * raw, size_t newSize)
{
    if (raw->size != newSize) {
        uint8_t * old = raw->data;
        size_t oldSize = raw->size;
        raw->data = avifAlloc(newSize);
        raw->size = newSize;
        if (oldSize) {
            size_t bytesToCopy = (oldSize < raw->size) ? oldSize : raw->size;
            memcpy(raw->data, old, bytesToCopy);
            avifFree(old);
        }
    }
}

void avifRWDataSet(avifRWData * raw, const uint8_t * data, size_t len)
{
    if (len) {
        avifRWDataRealloc(raw, len);
        memcpy(raw->data, data, len);
    } else {
        avifRWDataFree(raw);
    }
}

void avifRWDataFree(avifRWData * raw)
{
    avifFree(raw->data);
    raw->data = NULL;
    raw->size = 0;
}
