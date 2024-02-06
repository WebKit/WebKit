/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2023 Apple, Inc. All rights reserved.
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

#include "CanvasObserver.h"
#include "ExceptionOr.h"
#include "FloatRect.h"
#include "GraphicsTypesGL.h"
#include "PlatformXR.h"
#include "WebXRLayer.h"
#include <variant>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class HTMLCanvasElement;
class IntSize;
class WebGLFramebuffer;
class WebGLRenderingContext;
class WebGLRenderingContextBase;
class WebGL2RenderingContext;
class WebXROpaqueFramebuffer;
class WebXRSession;
class WebXRView;
class WebXRViewport;
struct XRWebGLLayerInit;

class WebXRWebGLLayer : public WebXRLayer, private CanvasObserver {
    WTF_MAKE_ISO_ALLOCATED(WebXRWebGLLayer);
public:

    using WebXRRenderingContext = std::variant<
        RefPtr<WebGLRenderingContext>,
        RefPtr<WebGL2RenderingContext>
    >;

    static ExceptionOr<Ref<WebXRWebGLLayer>> create(Ref<WebXRSession>&&, WebXRRenderingContext&&, const XRWebGLLayerInit&);
    ~WebXRWebGLLayer();

    bool antialias() const;
    bool ignoreDepthValues() const;

    const WebGLFramebuffer* framebuffer() const;
    unsigned framebufferWidth() const;
    unsigned framebufferHeight() const;

    ExceptionOr<RefPtr<WebXRViewport>> getViewport(WebXRView&);

    static double getNativeFramebufferScaleFactor(const WebXRSession&);

    const WebXRSession& session() { return m_session; }

    bool isCompositionEnabled() const { return m_isCompositionEnabled; }

    HTMLCanvasElement* canvas() const;

    // WebXRLayer
    void startFrame(const PlatformXR::FrameData&) final;
    PlatformXR::Device::Layer endFrame() final;

private:
    WebXRWebGLLayer(Ref<WebXRSession>&&, WebXRRenderingContext&&, std::unique_ptr<WebXROpaqueFramebuffer>&&, bool antialias, bool ignoreDepthValues, bool isCompositionEnabled);

    void computeViewports();
    static IntSize computeNativeWebGLFramebufferResolution();
    static IntSize computeRecommendedWebGLFramebufferResolution();

    void canvasChanged(CanvasBase&, const FloatRect&) final { };
    void canvasResized(CanvasBase&) final;
    void canvasDestroyed(CanvasBase&) final { };
    Ref<WebXRSession> m_session;
    WebXRRenderingContext m_context;

    struct ViewportData {
        Ref<WebXRViewport> viewport;
        double currentScale { 1.0 };
    };

    ViewportData m_leftViewportData;
    ViewportData m_rightViewportData;
    std::unique_ptr<WebXROpaqueFramebuffer> m_framebuffer;
    bool m_antialias { false };
    bool m_ignoreDepthValues { false };
    bool m_isCompositionEnabled { true };
    bool m_viewportsDirty { true };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
