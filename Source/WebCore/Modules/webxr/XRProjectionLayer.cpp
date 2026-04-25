/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
#include "WebXRRigidTransform.h"
#include "WebXRSession.h"
#include "XRLayerBacking.h"
#include <WebCore/IntSize.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRProjectionLayer);

XRProjectionLayer::XRProjectionLayer(ScriptExecutionContext& scriptExecutionContext, WebXRSession& session, Ref<XRLayerBacking>&& backing, const XRProjectionLayerInit& init)
    : XRCompositionLayer(&scriptExecutionContext, session, WTF::move(backing), init)
{
}

XRProjectionLayer::~XRProjectionLayer() = default;

void XRProjectionLayer::startFrame(PlatformXR::FrameData& data)
{
#if PLATFORM(COCOA)
    static constexpr auto defaultLayerHandle = 1;
    auto it = data.layers.find(defaultLayerHandle);
#else
    auto it = data.layers.find(m_backing->handle());
#endif
    if (it == data.layers.end()) {
        // For some reason the device didn't provide a texture for this frame.
        // The frame is ignored and the device can recover the texture in future frames;
        return;
    }

#if ENABLE(WEBGPU)
    auto& frameData = it->value;
    if (frameData->layerSetup && frameData->textureData) {
        m_layerData = frameData;
        auto& textureData = frameData->textureData;
        m_backing->startFrame(frameData->renderingFrameIndex, WTF::move(textureData->colorTexture.handle), WTF::move(textureData->depthStencilBuffer.handle), WTF::move(frameData->layerSetup->completionSyncEvent), textureData->reusableTextureIndex, WTF::move(frameData->layerSetup->foveationRateMapDesc));
    }
#else
    m_backing->startFrame(data);
#endif
}

#if ENABLE(WEBGPU)
std::optional<PlatformXR::FrameData::LayerData> XRProjectionLayer::layerData() const
{
    return m_layerData;
}
#endif

Vector<IntRect> XRProjectionLayer::computeViewports()
{
    auto roundDownShared = [](double value) -> int {
        return std::max(1, static_cast<int>(std::floor(value)));
    };

    auto width = m_backing->colorTextureWidth();
    auto height = m_backing->colorTextureHeight();

    if (!session() || !PlatformXR::isImmersive(session()->mode()) || session()->views().size() <= 1)
        return { { 0, 0, roundDownShared(width), roundDownShared(height) } };

    auto perViewWidth = roundDownShared(width / session()->views().size());
    auto perViewHeight = roundDownShared(height);

    Vector<IntRect> viewports;
    int viewportOriginX = 0;
    for (size_t i = 0; i < session()->views().size(); ++i) {
        viewports.append({ viewportOriginX, 0, perViewWidth, perViewHeight });
        viewportOriginX += perViewWidth;
    }

    return viewports;
}

PlatformXR::DeviceLayer XRProjectionLayer::endFrame()
{
    PlatformXR::DeviceLayer layerData;
#if PLATFORM(COCOA)
    m_backing->endFrame();
#else
    m_backing->endFrame(layerData);
#endif

    if (m_viewports.isEmpty()) [[unlikely]]
        m_viewports = computeViewports();
    ASSERT(m_viewports.size() == 1 || m_viewports.size() == 2);
    Vector<PlatformXR::DeviceLayer::LayerView> views(m_viewports.size());
    if (m_viewports.size() == 1)
        views[0] = { PlatformXR::Eye::None, m_viewports[0] };
    else {
        views[0] = { PlatformXR::Eye::Left, m_viewports[0] };
        views[1] = { PlatformXR::Eye::Right, m_viewports[1] };
    }

    layerData.handle = m_backing->handle();
    layerData.visible = true;
    layerData.views = WTF::move(views);

    return layerData;
}

uint32_t XRProjectionLayer::textureWidth() const
{
    return m_backing->colorTextureWidth();
}

uint32_t XRProjectionLayer::textureHeight() const
{
    return m_backing->colorTextureHeight();
}

uint32_t XRProjectionLayer::textureArrayLength() const
{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
    ASSERT(m_backing->colorTextureArrayLength() == 1);
#endif
    return m_backing->colorTextureArrayLength();
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

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
