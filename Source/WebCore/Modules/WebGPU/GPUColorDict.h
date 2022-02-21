/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <pal/graphics/WebGPU/WebGPUColor.h>
#include <variant>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct GPUColorDict {
    PAL::WebGPU::ColorDict convertToBacking() const
    {
        return {
            r,
            g,
            b,
            a,
        };
    }

    double r { 0 };
    double g { 0 };
    double b { 0 };
    double a { 0 };
};

using GPUColor = std::variant<Vector<double>, GPUColorDict>;

inline PAL::WebGPU::Color convertToBacking(const GPUColor& color)
{
    return WTF::switchOn(color, [] (const Vector<double>& vector) -> PAL::WebGPU::Color {
        return vector;
    }, [] (const GPUColorDict& color) -> PAL::WebGPU::Color {
        return color.convertToBacking();
    });
}

}
