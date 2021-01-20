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

#include "ExceptionOr.h"
#include "WebXRLayer.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>

namespace WebCore {

class IntSize;
class WebGLFramebuffer;
class WebGLRenderingContext;
#if ENABLE(WEBGL2)
class WebGL2RenderingContext;
#endif
class WebXRSession;
class WebXRView;
class WebXRViewport;
struct XRWebGLLayerInit;

class WebXRWebGLLayer : public WebXRLayer {
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

    WebGLFramebuffer* framebuffer() const;
    unsigned framebufferWidth() const;
    unsigned framebufferHeight() const;

    RefPtr<WebXRViewport> getViewport(const WebXRView&);

    static double getNativeFramebufferScaleFactor(const WebXRSession&);

    const WebXRSession& session() { return m_session; }

private:
    WebXRWebGLLayer(Ref<WebXRSession>&&, WebXRRenderingContext&&, const XRWebGLLayerInit&);

    static IntSize computeNativeWebGLFramebufferResolution();
    static IntSize computeRecommendedWebGLFramebufferResolution();

    Ref<WebXRSession> m_session;
    WebXRRenderingContext m_context;
    bool m_antialias { false };
    bool m_ignoreDepthValues { false };
    bool m_isCompositionDisabled { false };

    struct {
        RefPtr<WebGLFramebuffer> object;
        unsigned width { 0 };
        unsigned height { 0 };
    } m_framebuffer;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
