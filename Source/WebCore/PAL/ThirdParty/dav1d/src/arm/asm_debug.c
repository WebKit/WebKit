/*
 * Copyright © 2019-2026, Martin Storsjo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <inttypes.h>

void dav1d_dump_simd_s8(int line, const char *msg, int first, int last, const int8_t *ptr);
void dav1d_dump_simd_s8(int line, const char *msg, int first, int last, const int8_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int v = first; v <= last; v++) {
        const int8_t *vec = &ptr[16*v];
        fprintf(stderr, "v%02d.16b = {", v);
        for (int i = 0; i < 16; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#elif ARCH_ARM
    for (int v = first; v <= last; v++) {
        const int8_t *vec = &ptr[8*v];
        fprintf(stderr, "d%02d = {", v);
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#endif
    fflush(stderr);
}

void dav1d_dump_simd_u8(int line, const char *msg, int first, int last, const uint8_t *ptr);
void dav1d_dump_simd_u8(int line, const char *msg, int first, int last, const uint8_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int v = first; v <= last; v++) {
        const uint8_t *vec = &ptr[16*v];
        fprintf(stderr, "v%02d.16b = {", v);
        for (int i = 0; i < 16; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#elif ARCH_ARM
    for (int v = first; v <= last; v++) {
        const uint8_t *vec = &ptr[8*v];
        fprintf(stderr, "d%02d = {", v);
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#endif
    fflush(stderr);
}

void dav1d_dump_simd_s16(int line, const char *msg, int first, int last, const int16_t *ptr);
void dav1d_dump_simd_s16(int line, const char *msg, int first, int last, const int16_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int v = first; v <= last; v++) {
        const int16_t *vec = &ptr[8*v];
        fprintf(stderr, "v%02d.8h = {", v);
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#elif ARCH_ARM
    for (int v = first; v <= last; v++) {
        const int16_t *vec = &ptr[4*v];
        fprintf(stderr, "d%02d = {", v);
        for (int i = 0; i < 4; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#endif
    fflush(stderr);
}

void dav1d_dump_simd_u16(int line, const char *msg, int first, int last, const uint16_t *ptr);
void dav1d_dump_simd_u16(int line, const char *msg, int first, int last, const uint16_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int v = first; v <= last; v++) {
        const uint16_t *vec = &ptr[8*v];
        fprintf(stderr, "v%02d.8h = {", v);
        for (int i = 0; i < 8; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#elif ARCH_ARM
    for (int v = first; v <= last; v++) {
        const uint16_t *vec = &ptr[4*v];
        fprintf(stderr, "d%02d = {", v);
        for (int i = 0; i < 4; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#endif
    fflush(stderr);
}

void dav1d_dump_simd_s32(int line, const char *msg, int first, int last, const int32_t *ptr);
void dav1d_dump_simd_s32(int line, const char *msg, int first, int last, const int32_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int v = first; v <= last; v++) {
        const int32_t *vec = &ptr[4*v];
        fprintf(stderr, "v%02d.4s = {", v);
        for (int i = 0; i < 4; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#elif ARCH_ARM
    for (int v = first; v <= last; v++) {
        const int32_t *vec = &ptr[2*v];
        fprintf(stderr, "d%02d = {", v);
        for (int i = 0; i < 2; i++)
            fprintf(stderr, "%s %d", i > 0 ? "," : "", vec[i]);
        fprintf(stderr, " }\n");
    }
#endif
    fflush(stderr);
}

void dav1d_dump_gpr(int line, const char *msg, int first, int last, const size_t *ptr);
void dav1d_dump_gpr(int line, const char *msg, int first, int last, const size_t *ptr) {
    fprintf(stderr, "%d: %s:\n", line, msg);
#if ARCH_AARCH64
    for (int r = first; r <= last; r++)
        fprintf(stderr, "x%02d = %"PRId64" (0x%"PRIx64")\n", r, (int64_t) ptr[r], (uint64_t) ptr[r]);
#elif ARCH_ARM
    for (int r = first; r <= last; r++)
        if (r < 13)
            fprintf(stderr, "r%02d = %"PRId32" (0x%"PRIx32")\n", r, (int32_t) ptr[r], (uint32_t) ptr[r]);
        else
            fprintf(stderr, "%s  = %"PRId32" (0x%"PRIx32")\n", r == 13 ? "sp" : "lr", (int32_t) ptr[r], (uint32_t) ptr[r]);
#endif
    fflush(stderr);
}
