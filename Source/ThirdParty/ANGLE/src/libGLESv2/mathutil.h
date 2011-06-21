//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// mathutil.h: Math and bit manipulation functions.

#ifndef LIBGLESV2_MATHUTIL_H_
#define LIBGLESV2_MATHUTIL_H_

#include <intrin.h>
#include <math.h>
#include <windows.h>

namespace gl
{
inline bool isPow2(int x)
{
    return (x & (x - 1)) == 0 && (x != 0);
}

inline int log2(int x)
{
    int r = 0;
    while ((x >> r) > 1) r++;
    return r;
}

inline unsigned int ceilPow2(unsigned int x)
{
    if (x != 0) x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;

    return x;
}

template<typename T, typename MIN, typename MAX>
inline T clamp(T x, MIN min, MAX max)
{
    return x < min ? min : (x > max ? max : x);
}

inline float clamp01(float x)
{
    return clamp(x, 0.0f, 1.0f);
}

template<const int n>
inline unsigned int unorm(float x)
{
    const unsigned int max = 0xFFFFFFFF >> (32 - n);

    if (x > 1)
    {
        return max;
    }
    else if (x < 0)
    {
        return 0;
    }
    else
    {
        return (unsigned int)(max * x + 0.5f);
    }
}

inline RECT transformPixelRect(GLint x, GLint y, GLint w, GLint h, GLint surfaceHeight)
{
    RECT rect = {x,
                 surfaceHeight - y - h,
                 x + w,
                 surfaceHeight - y};
    return rect;
}

inline int transformPixelYOffset(GLint yoffset, GLint h, GLint surfaceHeight)
{
    return surfaceHeight - yoffset - h;
}

inline GLenum adjustWinding(GLenum winding)
{
    ASSERT(winding == GL_CW || winding == GL_CCW);
    return winding == GL_CW ? GL_CCW : GL_CW;
}

inline bool supportsSSE2()
{
    static bool checked = false;
    static bool supports = false;

    if (checked)
    {
        return supports;
    }

    int info[4];
    __cpuid(info, 0);
    
    if (info[0] >= 1)
    {
        __cpuid(info, 1);

        supports = (info[3] >> 26) & 1;
    }

    checked = true;

    return supports;
}
}

#endif   // LIBGLESV2_MATHUTIL_H_
