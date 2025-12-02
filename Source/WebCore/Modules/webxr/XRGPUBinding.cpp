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
#include "XRGPUBinding.h"

#if ENABLE(WEBXR_LAYERS) && ENABLE(WEBGPU)

#include "GPUDevice.h"
#include "WebGPUXRBinding.h"
#include "WebGPUXREye.h"
#include "WebGPUXRView.h"
#include "WebXRFrame.h"
#include "WebXRView.h"
#include "XRCompositionLayer.h"
#include "XRGPUProjectionLayerInit.h"
#include "XRGPUSubImage.h"
#include "XRProjectionLayer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static WebGPU::XREye convertToBacking(XREye eye)
{
    switch (eye) {
    case PlatformXR::Eye::None:
        return WebGPU::XREye::None;
    case PlatformXR::Eye::Left:
        return WebGPU::XREye::Left;
    case PlatformXR::Eye::Right:
        return WebGPU::XREye::Right;
    }
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(XRGPUBinding);

XRGPUBinding::XRGPUBinding(WebXRSession& session, GPUDevice& device)
    : m_backing(device.createXRBinding(session))
    , m_session(&session)
    , m_device(device)
{
}

GPUDevice& XRGPUBinding::device()
{
    return m_device;
}

ExceptionOr<Ref<XRProjectionLayer>> XRGPUBinding::createProjectionLayer(ScriptExecutionContext& scriptExecutionContext, std::optional<XRGPUProjectionLayerInit> init)
{
    if (!m_backing || !m_session)
        return Exception { ExceptionCode::AbortError };

    if (init) {
        auto converted = init->convertToBacking();
        m_session->initializeTrackingAndRendering(XRCanvasConfiguration {
            .colorFormat = converted.colorFormat,
            .depthStencilFormat = converted.depthStencilFormat
        });
    } else
        m_session->initializeTrackingAndRendering(std::nullopt);

    WebGPU::XRProjectionLayerInit convertedInit;
    if (init)
        convertedInit = init->convertToBacking();
    RefPtr projectionLayer = m_backing->createProjectionLayer(convertedInit);
    if (!projectionLayer)
        return Exception { ExceptionCode::AbortError };

    m_init = init;
    return XRProjectionLayer::create(scriptExecutionContext, projectionLayer.releaseNonNull());
}

double XRGPUBinding::nativeProjectionScaleFactor() const
{
    return m_init ? m_init->scaleFactor : 1.0;
}

ExceptionOr<Ref<XRGPUSubImage>> XRGPUBinding::getSubImage(XRProjectionLayer& projectionLayer, XREye eye)
{
    if (!m_backing)
        return Exception { ExceptionCode::AbortError };

    auto layerData = projectionLayer.layerData();
    if (!layerData)
        return Exception { ExceptionCode::AbortError, "First frame is not ready"_s };

    auto setupData = layerData->layerSetup;
    auto textureData = layerData->textureData;
    if (!setupData || !textureData)
        return Exception { ExceptionCode::AbortError, "Layer setup or texture data is missing"_s };

    unsigned eyeIndex = eye == XREye::Right ? 1 : 0;
    auto physicalSize = setupData->physicalSize[eyeIndex];
    if (!physicalSize[0] || !physicalSize[1])
        physicalSize = setupData->physicalSize[0];
    auto viewport = setupData->viewports[eyeIndex];
    if (eyeIndex)
        viewport.move(-setupData->viewports[0].width(), 0);

    RefPtr subImage = m_backing->getViewSubImage(static_cast<WebGPU::XRProjectionLayer&>(projectionLayer.backing()));
    return XRGPUSubImage::create(subImage.releaseNonNull(), convertToBacking(eye), WTFMove(physicalSize), WTFMove(viewport), m_device);
}

ExceptionOr<Ref<XRGPUSubImage>> XRGPUBinding::getSubImage(XRProjectionLayer& projectionLayer, WebXRFrame&, std::optional<XREye> eye)
{
    return getSubImage(projectionLayer, eye.value_or(XREye::Left));
}

ExceptionOr<Ref<XRGPUSubImage>> XRGPUBinding::getViewSubImage(XRProjectionLayer& projectionLayer, WebXRView& xrView)
{
    return getSubImage(projectionLayer, xrView.eye());
}

GPUTextureFormat XRGPUBinding::getPreferredColorFormat()
{
    return GPUTextureFormat::Bgra8unorm;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS) && ENABLE(WEBGPU)

