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

#if ENABLE(WEBXR_LAYERS)

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

XRGPUBinding::XRGPUBinding(const WebXRSession& session, GPUDevice& device)
    : m_backing(device.createXRBinding(session))
    , m_session(&session)
{
}

ExceptionOr<Ref<XRProjectionLayer>> XRGPUBinding::createProjectionLayer(ScriptExecutionContext& scriptExecutionContext, std::optional<XRGPUProjectionLayerInit> init)
{
    if (!m_backing)
        return Exception { ExceptionCode::AbortError };

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

RefPtr<XRGPUSubImage> XRGPUBinding::getSubImage(XRCompositionLayer&, WebXRFrame&, std::optional<XREye>/* = "none"*/)
{
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

ExceptionOr<Ref<XRGPUSubImage>> XRGPUBinding::getViewSubImage(XRProjectionLayer& projectionLayer, WebXRView& xrView)
{
    if (!m_backing)
        return Exception { ExceptionCode::AbortError };

    RefPtr subImage = m_backing->getViewSubImage(projectionLayer.backing(), convertToBacking(xrView.eye()));
    return XRGPUSubImage::create(subImage.releaseNonNull());
}

GPUTextureFormat XRGPUBinding::getPreferredColorFormat()
{
    return GPUTextureFormat::Bgra8unorm;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

