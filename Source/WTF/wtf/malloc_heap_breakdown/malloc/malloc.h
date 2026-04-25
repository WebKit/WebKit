/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#ifndef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* This file was inspired by malloc.h from MacOS. */

typedef struct _malloc_zone_t {
} malloc_zone_t;

typedef size_t vm_size_t;

/********* Creation and destruction ************/

extern malloc_zone_t *malloc_default_zone(void);
    /* The initial zone */

extern malloc_zone_t *malloc_create_zone(vm_size_t start_size, unsigned flags);
    /* Creates a new zone with default behavior and registers it */

/********* Block creation and manipulation ************/

extern void *malloc_zone_malloc(malloc_zone_t *zone, size_t size);
    /* Allocates a new pointer of size size; zone must be non-NULL */

extern void *malloc_zone_calloc(malloc_zone_t *zone, size_t num_items, size_t size);
    /* Allocates a new pointer of size num_items * size; block is cleared; zone must be non-NULL */

extern void malloc_zone_free(malloc_zone_t *zone, void *ptr);
    /* Frees pointer in zone; zone must be non-NULL */

extern void *malloc_zone_realloc(malloc_zone_t *zone, void *ptr, size_t size);
    /* Enlarges block if necessary; zone must be non-NULL */

extern void *malloc_zone_memalign(malloc_zone_t *zone, size_t alignment, size_t size);
    /*
     * Allocates a new pointer of size size whose address is an exact multiple of alignment.
     * alignment must be a power of two and at least as large as sizeof(void *).
     * zone must be non-NULL.
     */

/********* Functions for zone implementors ************/

extern void malloc_set_zone_name(malloc_zone_t *zone, const char *name);
    /* Sets the name of a zone */

size_t malloc_zone_pressure_relief(malloc_zone_t *zone, size_t goal);
    /* malloc_zone_pressure_relief() advises the malloc subsystem that the process is under memory pressure and
     * that the subsystem should make its best effort towards releasing (i.e. munmap()-ing) "goal" bytes from "zone".
     * If "goal" is passed as zero, the malloc subsystem will attempt to achieve maximal pressure relief in "zone".
     * If "zone" is passed as NULL, all zones are examined for pressure relief opportunities.
     * malloc_zone_pressure_relief() returns the number of bytes released.
     */

/********* Debug helpers ************/

extern void malloc_zone_print(malloc_zone_t *zone, bool verbose);
    /* Prints summary on zone; if !zone, prints all zones */

#ifndef __cplusplus
}
#endif
