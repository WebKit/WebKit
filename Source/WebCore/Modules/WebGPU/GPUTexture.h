/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "GPUIntegralTypes.h"
#include "GPUTextureDimension.h"
#include "GPUTextureFormat.h"
#include "WebGPUTexture.h"
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUDevice;
class GPUTextureView;

struct GPUTextureDescriptor;
struct GPUTextureViewDescriptor;

class GPUTexture : public RefCounted<GPUTexture> {
public:
    static Ref<GPUTexture> create(Ref<WebGPU::Texture>&& backing, const GPUTextureDescriptor& descriptor, const GPUDevice& device)
    {
        return adoptRef(*new GPUTexture(WTFMove(backing), descriptor, device));
    }

    String label() const;
    void setLabel(String&&);

    ExceptionOr<Ref<GPUTextureView>> createView(const std::optional<GPUTextureViewDescriptor>&) const;

    void destroy();

    WebGPU::Texture& backing() { return m_backing; }
    const WebGPU::Texture& backing() const { return m_backing; }
    GPUTextureFormat format() const { return m_format; }

    GPUIntegerCoordinateOut width() const;
    GPUIntegerCoordinateOut height() const;
    GPUIntegerCoordinateOut depthOrArrayLayers() const;
    GPUIntegerCoordinateOut mipLevelCount() const;
    GPUSize32Out sampleCount() const;
    GPUTextureDimension dimension() const;
    GPUFlagsConstant usage() const;

    virtual ~GPUTexture();
private:
    GPUTexture(Ref<WebGPU::Texture>&&, const GPUTextureDescriptor&, const GPUDevice&);

    GPUTexture(const GPUTexture&) = delete;
    GPUTexture(GPUTexture&&) = delete;
    GPUTexture& operator=(const GPUTexture&) = delete;
    GPUTexture& operator=(GPUTexture&&) = delete;

    Ref<WebGPU::Texture> m_backing;
    const GPUTextureFormat m_format;
    const GPUIntegerCoordinateOut m_width;
    const GPUIntegerCoordinateOut m_height;
    const GPUIntegerCoordinateOut m_depthOrArrayLayers;
    const GPUIntegerCoordinateOut m_mipLevelCount;
    const GPUSize32Out m_sampleCount;
    const GPUTextureDimension m_dimension;
    const GPUFlagsConstant m_usage;
    Ref<const GPUDevice> m_device;
};

}
