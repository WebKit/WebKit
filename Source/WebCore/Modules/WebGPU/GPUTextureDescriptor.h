/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "GPUExtent3DDict.h"
#include "GPUIntegralTypes.h"
#include "GPUObjectDescriptorBase.h"
#include "GPUTextureDimension.h"
#include "GPUTextureFormat.h"
#include "GPUTextureUsage.h"
#include <pal/graphics/WebGPU/WebGPUTextureDescriptor.h>

namespace WebCore {

struct GPUTextureDescriptor : public GPUObjectDescriptorBase {
    PAL::WebGPU::TextureDescriptor convertToBacking() const
    {
        return {
            { label },
            WebCore::convertToBacking(size),
            mipLevelCount,
            sampleCount,
            WebCore::convertToBacking(dimension),
            WebCore::convertToBacking(format),
            convertTextureUsageFlagsToBacking(usage),
            viewFormats.map([] (auto viewFormat) {
                return WebCore::convertToBacking(viewFormat);
            }),
        };
    }

    GPUExtent3D size;
    GPUIntegerCoordinate mipLevelCount { 1 };
    GPUSize32 sampleCount { 1 };
    GPUTextureDimension dimension { GPUTextureDimension::_2d };
    GPUTextureFormat format { GPUTextureFormat::R8unorm };
    GPUTextureUsageFlags usage { 0 };
    Vector<GPUTextureFormat> viewFormats;
};

}
