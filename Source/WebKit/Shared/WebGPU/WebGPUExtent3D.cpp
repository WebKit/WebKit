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
#include "WebGPUExtent3D.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUExtent3D.h>

namespace WebKit::WebGPU {

std::optional<Extent3DDict> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::Extent3DDict& extent3DDict)
{
    return { { extent3DDict.width, extent3DDict.height, extent3DDict.depthOrArrayLayers } };
}

std::optional<Extent3D> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::Extent3D& extent3D)
{
    return WTF::switchOn(extent3D, [] (const Vector<PAL::WebGPU::IntegerCoordinate>& vector) -> std::optional<Extent3D> {
        return { { vector } };
    }, [this] (const PAL::WebGPU::Extent3DDict& extent3DDict) -> std::optional<Extent3D> {
        auto extent3D = convertToBacking(extent3DDict);
        if (!extent3D)
            return std::nullopt;
        return { { WTFMove(*extent3D) } };
    });
}

std::optional<PAL::WebGPU::Extent3DDict> ConvertFromBackingContext::convertFromBacking(const Extent3DDict& extent3DDict)
{
    return { { extent3DDict.width, extent3DDict.height, extent3DDict.depthOrArrayLayers } };
}

std::optional<PAL::WebGPU::Extent3D> ConvertFromBackingContext::convertFromBacking(const Extent3D& extent3D)
{
    return WTF::switchOn(extent3D, [] (const Vector<PAL::WebGPU::IntegerCoordinate>& vector) -> std::optional<PAL::WebGPU::Extent3D> {
        return { { vector } };
    }, [this] (const Extent3DDict& extent3DDict) -> std::optional<PAL::WebGPU::Extent3D> {
        auto extent3D = convertFromBacking(extent3DDict);
        if (!extent3D)
            return std::nullopt;
        return { { WTFMove(*extent3D) } };
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
