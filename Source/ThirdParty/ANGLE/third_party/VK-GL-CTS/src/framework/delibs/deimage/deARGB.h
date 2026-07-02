#ifndef _DEARGB_H
#define _DEARGB_H
/*-------------------------------------------------------------------------
 * drawElements Image Library
 * --------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Image library.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deInt32.h"

/* ARGB color in descending A, R, G, B order (alpha is in most significant byte). */
typedef uint32_t deARGB;

static inline deARGB deARGB_set(int r, int g, int b, int a)
{
    DE_ASSERT(deInBounds32(r, 0, 256));
    DE_ASSERT(deInBounds32(g, 0, 256));
    DE_ASSERT(deInBounds32(b, 0, 256));
    DE_ASSERT(deInBounds32(a, 0, 256));
    return (a << 24) | (r << 16) | (g << 8) | (b << 0);
}

static inline deARGB deARGB_white(void)
{
    return deARGB_set(0xFF, 0xFF, 0xFF, 0xFF);
}
static inline deARGB deARGB_black(void)
{
    return deARGB_set(0, 0, 0, 0xFF);
}

static inline int deARGB_getRed(deARGB argb)
{
    return (int)((argb >> 16) & 0xFF);
}
static inline int deARGB_getGreen(deARGB argb)
{
    return (int)((argb >> 8) & 0xFF);
}
static inline int deARGB_getBlue(deARGB argb)
{
    return (int)((argb >> 0) & 0xFF);
}
static inline int deARGB_getAlpha(deARGB argb)
{
    return (int)((argb >> 24) & 0xFF);
}

static inline deARGB deARGB_multiply(deARGB argb, int f)
{
    DE_ASSERT(deInRange32(f, 0, 256));
    {
        int r = (deARGB_getRed(argb) * f + 128) >> 8;
        int g = (deARGB_getGreen(argb) * f + 128) >> 8;
        int b = (deARGB_getBlue(argb) * f + 128) >> 8;
        int a = (deARGB_getAlpha(argb) * f + 128) >> 8;
        return deARGB_set(r, g, b, a);
    }
}

static inline deARGB deARGB_add(deARGB a, deARGB b)
{
    return deARGB_set(deClamp32(deARGB_getRed(a) + deARGB_getRed(b), 0, 255),
                      deClamp32(deARGB_getGreen(a) + deARGB_getGreen(b), 0, 255),
                      deClamp32(deARGB_getBlue(a) + deARGB_getBlue(b), 0, 255),
                      deClamp32(deARGB_getAlpha(a) + deARGB_getAlpha(b), 0, 255));
}

#endif /* _DEARGB_H */
