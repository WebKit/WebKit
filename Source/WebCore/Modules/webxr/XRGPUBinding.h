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

#pragma once

#if ENABLE(WEBXR_LAYERS)

#include "GPUTextureFormat.h"
#include "WebGPUXRBinding.h"
#include "WebXRSession.h"
#include "XREye.h"
#include "XRGPUProjectionLayerInit.h"

#include <ExceptionOr.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

namespace WebGPU {
class XRBinding;
}

enum class GPUTextureFormat : uint8_t;

class GPUDevice;
class WebXRFrame;
class WebXRSession;
class WebXRView;
class XRCompositionLayer;
class XRCubeLayer;
class XRCylinderLayer;
class XREquirectLayer;
class XRProjectionLayer;
class XRQuadLayer;
class XRGPUSubImage;

struct XRCubeLayerInit;
struct XRCylinderLayerInit;
struct XREquirectLayerInit;
struct XRGPUProjectionLayerInit;
struct XRProjectionLayerInit;
struct XRQuadLayerInit;

// https://github.com/immersive-web/WebXR-WebGPU-Binding/blob/main/explainer.md
class XRGPUBinding : public RefCounted<XRGPUBinding> {
    WTF_MAKE_ISO_ALLOCATED(XRGPUBinding);
public:
    static Ref<XRGPUBinding> create(const WebXRSession& session, GPUDevice& device)
    {
        return adoptRef(*new XRGPUBinding(session, device));
    }

    double nativeProjectionScaleFactor() const;

    ExceptionOr<Ref<XRProjectionLayer>> createProjectionLayer(ScriptExecutionContext&, std::optional<XRGPUProjectionLayerInit>);
    RefPtr<XRGPUSubImage> getSubImage(XRCompositionLayer&, WebXRFrame&, std::optional<XREye>/* = "none"*/);
    ExceptionOr<Ref<XRGPUSubImage>> getViewSubImage(XRProjectionLayer&, WebXRView&);
    GPUTextureFormat getPreferredColorFormat();

    GPUDevice& device();

    // The core specification doesn't require these, support will be added later.
    // XRQuadLayer createQuadLayer(optional XRGPUQuadLayerInit init);
    // XRCylinderLayer createCylinderLayer(optional XRGPUCylinderLayerInit init);
    // XREquirectLayer createEquirectLayer(optional XRGPUEquirectLayerInit init);
    // XRCubeLayer createCubeLayer(optional XRGPUCubeLayerInit init);
private:
    XRGPUBinding(const WebXRSession&, GPUDevice&);

    RefPtr<WebGPU::XRBinding> m_backing;
    RefPtr<const WebXRSession> m_session;
    std::optional<XRGPUProjectionLayerInit> m_init;
    Ref<GPUDevice> m_device;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
