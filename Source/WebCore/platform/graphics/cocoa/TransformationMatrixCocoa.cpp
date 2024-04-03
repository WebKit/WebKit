/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TransformationMatrix.h"

#if PLATFORM(COCOA)

#include <simd/simd.h>

namespace WebCore {

TransformationMatrix::TransformationMatrix(const simd_float4x4& t)
: TransformationMatrix(t.columns[0][0], t.columns[0][1], t.columns[0][2], t.columns[0][3],
    t.columns[1][0], t.columns[1][1], t.columns[1][2], t.columns[1][3],
    t.columns[2][0], t.columns[2][1], t.columns[2][2], t.columns[2][3],
    t.columns[3][0], t.columns[3][1], t.columns[3][2], t.columns[3][3])
{
}

TransformationMatrix::operator simd_float4x4() const
{
    return simd_float4x4 {
        simd_float4 { (float)m11(), (float)m12(), (float)m13(), (float)m14() },
        simd_float4 { (float)m21(), (float)m22(), (float)m23(), (float)m24() },
        simd_float4 { (float)m31(), (float)m32(), (float)m33(), (float)m34() },
        simd_float4 { (float)m41(), (float)m42(), (float)m43(), (float)m44() }
    };
}

}

#endif // PLATFORM(COCOA)
