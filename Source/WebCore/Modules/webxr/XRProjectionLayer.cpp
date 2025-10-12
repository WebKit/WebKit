/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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
#include "XRProjectionLayer.h"

#if ENABLE(WEBXR_LAYERS)

#include "PlatformXR.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(XRProjectionLayer);

#if ENABLE(WEBGPU)
XRProjectionLayer::XRProjectionLayer(ScriptExecutionContext& scriptExecutionContext, Ref<WebCore::WebGPU::XRProjectionLayer>&& backing)
    : XRCompositionLayer(&scriptExecutionContext)
    , m_backing(WTFMove(backing))
{
}
#endif

XRProjectionLayer::~XRProjectionLayer() = default;

void XRProjectionLayer::startFrame(PlatformXR::FrameData& data)
{
#if ENABLE(WEBGPU)
    static constexpr auto defaultLayerHandle = 1;
    auto it = data.layers.find(defaultLayerHandle);
    if (it == data.layers.end()) {
        // For some reason the device didn't provide a texture for this frame.
        // The frame is ignored and the device can recover the texture in future frames;
        return;
    }

    auto& frameData = it->value;
    if (frameData->layerSetup && frameData->textureData) {
        m_layerData = frameData;
        auto& textureData = frameData->textureData;
        m_backing->startFrame(frameData->renderingFrameIndex, WTFMove(textureData->colorTexture.handle), WTFMove(textureData->depthStencilBuffer.handle), WTFMove(frameData->layerSetup->completionSyncEvent), textureData->reusableTextureIndex, WTFMove(frameData->layerSetup->foveationRateMapDesc));
    }
#else
    UNUSED_PARAM(data);
#endif
}

#if ENABLE(WEBGPU)
std::optional<PlatformXR::FrameData::LayerData> XRProjectionLayer::layerData() const
{
    return m_layerData;
}
#endif

PlatformXR::Device::Layer XRProjectionLayer::endFrame()
{
#if ENABLE(WEBGPU)
    m_backing->endFrame();
#endif
    return PlatformXR::Device::Layer {
        .handle = 0,
        .visible = true,
        .views = { },
#if PLATFORM(GTK) || PLATFORM(WPE)
        .fenceFD = { }
#endif
    };
}

uint32_t XRProjectionLayer::textureWidth() const
{
#if ENABLE(WEBGPU)
    return m_backing->textureWidth();
#endif
    return 0;
}

uint32_t XRProjectionLayer::textureHeight() const
{
#if ENABLE(WEBGPU)
    return m_backing->textureHeight();
#endif
    return 0;
}

uint32_t XRProjectionLayer::textureArrayLength() const
{
#if ENABLE(WEBGPU)
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    ASSERT(m_backing->textureArrayLength() == 1);
#else
    ASSERT(m_backing->textureArrayLength() == 2);
#endif
    return m_backing->textureArrayLength();
#endif
    return 0;
}

bool XRProjectionLayer::ignoreDepthValues() const
{
    return false;
}

std::optional<float> XRProjectionLayer::fixedFoveation() const
{
    return 1.0;
}

void XRProjectionLayer::setFixedFoveation(std::optional<float>)
{
}

WebXRRigidTransform* XRProjectionLayer::deltaPose() const
{
    return m_transform.get();
}

void XRProjectionLayer::setDeltaPose(WebXRRigidTransform* deltaPose)
{
    m_transform = deltaPose;
}

#if ENABLE(WEBGPU)
WebCore::WebGPU::XRProjectionLayer& XRProjectionLayer::backing()
{
    return m_backing;
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
