// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/avif.h"

#include <stdlib.h>

void * avifAlloc(size_t size)
{
    void * out = malloc(size);
    if (out == NULL) {
        abort();
    }
    return out;
}

void avifFree(void * p)
{
    free(p);
}
