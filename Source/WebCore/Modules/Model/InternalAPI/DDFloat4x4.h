/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)
#include <simd/simd.h>
#endif

namespace WebCore::DDModel {

struct DDFloat4x4 {
    union {
#if PLATFORM(COCOA)
        simd_float4x4 v;
#endif
        struct {
            float m00, m01, m02, m03;
            float m10, m11, m12, m13;
            float m20, m21, m22, m23;
            float m30, m31, m32, m33;
        };
    };
#if PLATFORM(COCOA)
    DDFloat4x4(const simd_float4x4& s)
        : v(s)
    {
    }

    operator simd_float4x4() { return v; } // NOLINT
    operator const simd_float4x4() const { return v; } // NOLINT
#endif

    DDFloat4x4(float vm00, float vm01, float vm02, float vm03,
        float vm10, float vm11, float vm12, float vm13,
        float vm20, float vm21, float vm22, float vm23,
        float vm30, float vm31, float vm32, float vm33)
            : m00(vm00), m01(vm01), m02(vm02), m03(vm03)
            , m10(vm10), m11(vm11), m12(vm12), m13(vm13)
            , m20(vm20), m21(vm21), m22(vm22), m23(vm23)
            , m30(vm30), m31(vm31), m32(vm32), m33(vm33)
    {
    }
};

}
