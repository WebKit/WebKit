/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(CORE_RE)

#include <CoreRE/CoreRE.h>

#else

#include <math.h>
#include <simd/simd.h>

typedef struct {
    simd_float3 scale;
    simd_quatf rotation;
    simd_float3 translation;
} RESRT;

inline RESRT RESRTIdentity()
{
    return { simd_make_float3(1, 1, 1), simd_quaternion(0.f, 0.f, 0.f, 1.f), simd_make_float3(0, 0, 0) };
}

inline RESRT REMakeSRT(simd_float3 scale, simd_quatf rotation, simd_float3 translation)
{
    return { scale, rotation, translation };
}

inline RESRT REMakeSRTFromMatrix(simd_float4x4 m)
{
    simd_float3 translation = simd_make_float3(m.columns[3].x, m.columns[3].y, m.columns[3].z);

    simd_float3 col0 = simd_make_float3(m.columns[0].x, m.columns[0].y, m.columns[0].z);
    simd_float3 col1 = simd_make_float3(m.columns[1].x, m.columns[1].y, m.columns[1].z);
    simd_float3 col2 = simd_make_float3(m.columns[2].x, m.columns[2].y, m.columns[2].z);

    float sx = simd_length(col0);
    float sy = simd_length(col1);
    float sz = simd_length(col2);

    simd_float3x3 upper = simd_matrix(col0, col1, col2);
    if (simd_determinant(upper) < 0)
        sx = -sx;

    simd_float3 scale = simd_make_float3(sx, sy, sz);

    if (sx)
        col0 /= sx;
    if (sy)
        col1 /= sy;
    if (sz)
        col2 /= sz;

    simd_float3x3 rotMatrix = simd_matrix(col0, col1, col2);
    simd_quatf rotation = simd_quaternion(rotMatrix);

    return { scale, rotation, translation };
}

inline simd_float4x4 RESRTMatrix(RESRT srt)
{
    simd_float4x4 m = simd_matrix4x4(srt.rotation);

    m.columns[0] *= srt.scale.x;
    m.columns[1] *= srt.scale.y;
    m.columns[2] *= srt.scale.z;

    m.columns[3] = simd_make_float4(srt.translation.x, srt.translation.y, srt.translation.z, 1.0f);

    return m;
}

#endif // HAVE(CORE_RE)
