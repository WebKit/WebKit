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

#include "config.h"
#include "WebGPUOrigin2D.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUOrigin2D.h>

namespace WebKit::WebGPU {

std::optional<Origin2DDict> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::Origin2DDict& origin2DDict)
{
    return { { origin2DDict.x, origin2DDict.y } };
}

std::optional<Origin2D> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::Origin2D& origin2D)
{
    return WTF::switchOn(origin2D, [] (const Vector<PAL::WebGPU::IntegerCoordinate>& vector) -> std::optional<Origin2D> {
        return { { vector } };
    }, [this] (const PAL::WebGPU::Origin2DDict& origin2DDict) -> std::optional<Origin2D> {
        auto origin2D = convertToBacking(origin2DDict);
        if (!origin2D)
            return std::nullopt;
        return { { WTFMove(*origin2D) } };
    });
}

std::optional<PAL::WebGPU::Origin2DDict> ConvertFromBackingContext::convertFromBacking(const Origin2DDict& origin2DDict)
{
    return { { origin2DDict.x, origin2DDict.y } };
}

std::optional<PAL::WebGPU::Origin2D> ConvertFromBackingContext::convertFromBacking(const Origin2D& origin2D)
{
    return WTF::switchOn(origin2D, [] (const Vector<PAL::WebGPU::IntegerCoordinate>& vector) -> std::optional<PAL::WebGPU::Origin2D> {
        return { { vector } };
    }, [this] (const Origin2DDict& origin2DDict) -> std::optional<PAL::WebGPU::Origin2D> {
        auto origin2D = convertFromBacking(origin2DDict);
        if (!origin2D)
            return std::nullopt;
        return { { WTFMove(*origin2D) } };
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
