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

#include "CanvasBase.h"
#include "ExceptionOr.h"
#include "FloatRect.h"
#include "GraphicsTypesGL.h"
#include "PlatformXR.h"
#include "WebXRLayer.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>

namespace WebCore {

class HTMLCanvasElement;
class IntSize;
class WebGLFramebuffer;
class WebGLRenderingContext;
class WebGLRenderingContextBase;
#if ENABLE(WEBGL2)
class WebGL2RenderingContext;
#endif
class WebXROpaqueFramebuffer;
class WebXRSession;
class WebXRView;
class WebXRViewport;
struct XRWebGLLayerInit;

class WebXRWebGLLayer : public WebXRLayer, private CanvasObserver {
    WTF_MAKE_ISO_ALLOCATED(WebXRWebGLLayer);
public:

    using WebXRRenderingContext = WTF::Variant<
        RefPtr<WebGLRenderingContext>
#if ENABLE(WEBGL2)
        , RefPtr<WebGL2RenderingContext>
#endif
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
    void startFrame(const PlatformXR::Device::FrameData&) final;
    PlatformXR::Device::Layer endFrame() final;

private:
    WebXRWebGLLayer(Ref<WebXRSession>&&, WebXRRenderingContext&&, std::unique_ptr<WebXROpaqueFramebuffer>&&, bool antialias, bool ignoreDepthValues, bool isCompositionEnabled);

    void computeViewports();
    static IntSize computeNativeWebGLFramebufferResolution();
    static IntSize computeRecommendedWebGLFramebufferResolution();

    void canvasChanged(CanvasBase&, const Optional<FloatRect>&) final { };
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

class WebXROpaqueFramebuffer {
public:
    struct Attributes {
        bool alpha { true };
        bool antialias { true };
        bool depth { true };
        bool stencil { false };
    };

    static std::unique_ptr<WebXROpaqueFramebuffer> create(PlatformXR::LayerHandle, WebGLRenderingContextBase&, Attributes&&, uint32_t width, uint32_t height);
    ~WebXROpaqueFramebuffer();

    PlatformXR::LayerHandle handle() const { return m_handle; }
    const WebGLFramebuffer& framebuffer() const { return m_framebuffer.get(); }
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    void startFrame(const PlatformXR::Device::FrameData::LayerData&);
    void endFrame();

private:
    WebXROpaqueFramebuffer(PlatformXR::LayerHandle, Ref<WebGLFramebuffer>&&, WebGLRenderingContextBase&, Attributes&&, uint32_t width, uint32_t height);

    bool setupFramebuffer();

    PlatformXR::LayerHandle m_handle;
    Ref<WebGLFramebuffer> m_framebuffer;
    WebGLRenderingContextBase& m_context;
    Attributes m_attributes;
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    PlatformGLObject m_depthStencilBuffer { 0 };
    PlatformGLObject m_stencilBuffer { 0 };
    PlatformGLObject m_multisampleColorBuffer { 0 };
    PlatformGLObject m_resolvedFBO { 0 };
    GCGLint m_sampleCount { 0 };
    PlatformGLObject m_opaqueTexture { 0 };
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
