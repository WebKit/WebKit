//
// Copyright 2019 The ANGLE Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// common.h: Common header for other metal source code.

#ifndef LIBANGLE_RENDERER_METAL_SHADERS_COMMON_H_
#define LIBANGLE_RENDERER_METAL_SHADERS_COMMON_H_

#ifndef SKIP_STD_HEADERS
#    include <simd/simd.h>
#    include <metal_stdlib>
#endif

#define ANGLE_KERNEL_GUARD(IDX, MAX_COUNT) \
    if (IDX >= MAX_COUNT)                  \
    {                                      \
        return;                            \
    }

using namespace metal;

// Full screen quad's vertices
constant float2 gCorners[6] = {
    float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(-1.0f, -1.0f),
    float2(-1.0f, 1.0f), float2(1.0f, 1.0f),  float2(1.0f, -1.0f),
};

// Full screen quad's texcoords indices:
// 0: lower left, 1: lower right, 2: upper left, 3: upper right
constant int gTexcoordsIndices[6] = {2, 1, 0, 2, 3, 1};

fragment float4 dummyFS()
{
    return float4(0, 0, 0, 0);
}
#endif /* LIBANGLE_RENDERER_METAL_SHADERS_COMMON_H_ */
