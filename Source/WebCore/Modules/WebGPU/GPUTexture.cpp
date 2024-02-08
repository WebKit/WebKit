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

#include "config.h"
#include "GPUTexture.h"

#include "GPUDevice.h"
#include "GPUTextureDescriptor.h"
#include "GPUTextureView.h"
#include "GPUTextureViewDescriptor.h"
#include "WebGPUTextureViewDescriptor.h"

namespace WebCore {

template<int dimension>
static uint32_t getDimension(auto& extent3D)
{
    static_assert(dimension >= 0 && dimension <= 2, "dimension must be 0, 1, or 2");
    return WTF::switchOn(extent3D, [&](const Vector<GPUIntegerCoordinate>& vector) {
        return dimension <vector.size() ? vector[dimension] : 1;
    }, [&](const GPUExtent3DDict& extent3DDict) {
        switch (dimension) {
        case 0:
            return extent3DDict.width;
        case 1:
            return extent3DDict.height;
        case 2:
            return extent3DDict.depthOrArrayLayers;
        default:
            ASSERT_NOT_REACHED();
            return 0u;
        }
    });
}

GPUTexture::GPUTexture(Ref<WebGPU::Texture>&& backing, const GPUTextureDescriptor& descriptor, const GPUDevice& device)
    : m_backing(WTFMove(backing))
    , m_format(descriptor.format)
    , m_width(getDimension<0>(descriptor.size))
    , m_height(getDimension<1>(descriptor.size))
    , m_depthOrArrayLayers(getDimension<2>(descriptor.size))
    , m_mipLevelCount(descriptor.mipLevelCount)
    , m_sampleCount(descriptor.sampleCount)
    , m_dimension(descriptor.dimension)
    , m_usage(descriptor.usage)
    , m_device(device)
{
}

GPUTexture::~GPUTexture() = default;

String GPUTexture::label() const
{
    return m_backing->label();
}

void GPUTexture::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

static WebGPU::TextureViewDescriptor convertToBacking(const std::optional<GPUTextureViewDescriptor>& textureViewDescriptor)
{
    if (!textureViewDescriptor) {
        return {
            { },
            std::nullopt,
            std::nullopt,
            WebGPU::TextureAspect::All,
            0,
            std::nullopt,
            0,
            std::nullopt
        };
    }
    return textureViewDescriptor->convertToBacking();
}

ExceptionOr<Ref<GPUTextureView>> GPUTexture::createView(const std::optional<GPUTextureViewDescriptor>& textureViewDescriptor) const
{
    if (textureViewDescriptor.has_value() && textureViewDescriptor->format.has_value()) {
        if (!m_device->isSupportedFormat(*textureViewDescriptor->format))
            return Exception { ExceptionCode::TypeError, "Unsupported texture format."_s };
    }
    return GPUTextureView::create(m_backing->createView(convertToBacking(textureViewDescriptor)));
}

void GPUTexture::destroy()
{
    m_backing->destroy();
}

GPUIntegerCoordinateOut GPUTexture::width() const
{
    return m_width;
}

GPUIntegerCoordinateOut GPUTexture::height() const
{
    return m_height;
}

GPUIntegerCoordinateOut GPUTexture::depthOrArrayLayers() const
{
    return m_depthOrArrayLayers;
}

GPUIntegerCoordinateOut GPUTexture::mipLevelCount() const
{
    return m_mipLevelCount;
}

GPUSize32Out GPUTexture::sampleCount() const
{
    return m_sampleCount;
}

GPUTextureDimension GPUTexture::dimension() const
{
    return m_dimension;
}

GPUFlagsConstant GPUTexture::usage() const
{
    return m_usage;
}

}
