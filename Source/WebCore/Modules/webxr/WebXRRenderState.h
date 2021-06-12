/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "HTMLCanvasElement.h"
#include "WebXRWebGLLayer.h"
#include "XRSessionMode.h"

namespace WebCore {

struct XRRenderStateInit;

class WebXRRenderState : public RefCounted<WebXRRenderState> {
    WTF_MAKE_ISO_ALLOCATED(WebXRRenderState);
public:
    static Ref<WebXRRenderState> create(XRSessionMode);
    ~WebXRRenderState();

    Ref<WebXRRenderState> clone() const;

    double depthNear() const { return m_depth.near; }
    void setDepthNear(double near) { m_depth.near = near; }

    double depthFar() const { return m_depth.far; }
    void setDepthFar(double far) { m_depth.far = far; };

    std::optional<double> inlineVerticalFieldOfView() const { return m_inlineVerticalFieldOfView; }
    void setInlineVerticalFieldOfView(double fieldOfView) { m_inlineVerticalFieldOfView = fieldOfView; }

    RefPtr<WebXRWebGLLayer> baseLayer() const { return m_baseLayer; }
    void setBaseLayer(WebXRWebGLLayer* baseLayer) { m_baseLayer = baseLayer; }

    HTMLCanvasElement* outputCanvas() const { return m_outputCanvas.get(); }
    void setOutputCanvas(HTMLCanvasElement* canvas) { m_outputCanvas = makeWeakPtr(canvas); }

    bool isCompositionEnabled() const { return m_compositionEnabled; }
    void setCompositionEnabled(bool compositionEnabled) { m_compositionEnabled = compositionEnabled; }

private:
    explicit WebXRRenderState(std::optional<double> fieldOfView);
    explicit WebXRRenderState(const WebXRRenderState&);

    // https://immersive-web.github.io/webxr/#initialize-the-render-state
    struct {
        double near { 0.1 }; // in meters
        double far { 1000 }; // in meters
    } m_depth;
    std::optional<double> m_inlineVerticalFieldOfView; // in radians
    RefPtr<WebXRWebGLLayer> m_baseLayer;
    WeakPtr<HTMLCanvasElement> m_outputCanvas;
    bool m_compositionEnabled { true };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
