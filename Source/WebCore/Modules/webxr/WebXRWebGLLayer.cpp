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

#include "config.h"
#include "WebXRWebGLLayer.h"

#if ENABLE(WEBXR)

#include "WebGLFramebuffer.h"
#include "WebGLRenderingContext.h"
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#include "WebXRSession.h"
#include "WebXRViewport.h"
#include "XRWebGLLayerInit.h"

namespace WebCore {

ExceptionOr<Ref<WebXRWebGLLayer>> WebXRWebGLLayer::create(const WebXRSession& session, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
{
    // The XRWebGLLayer(session, context, layerInit) constructor MUST perform the following steps when invoked:
    //   1. Let layer be a new XRWebGLLayer

    //   2. If session’s ended value is true, throw an InvalidStateError and abort these steps.
    if (session.ended())
        return Exception { InvalidStateError };

    //   3. If context is lost, throw an InvalidStateError and abort these steps.
    //   4. If context’s XR compatible boolean is false, throw an InvalidStateError and abort these steps.
    // FIXME: TODO

    //   5. Initialize layer’s context to context.
    //   6. Initialize layer’s antialias to layerInit’s antialias value.
    //   7. If layerInit’s ignoreDepthValues value is false and the XR Compositor will make use of depth values, Initialize layer’s ignoreDepthValues to false.
    //   8. Else Initialize layer’s ignoreDepthValues to true
    //   9. Initialize layer’s framebuffer to a new opaque framebuffer created with context.
    //   10. Initialize the layer’s swap chain.
    //   11. If layer’s swap chain was unable to be created for any reason, throw an OperationError and abort these steps.
    // FIXME: TODO

    //   12. Return layer.
    return adoptRef(*new WebXRWebGLLayer(session, WTFMove(context), init));
}

WebXRWebGLLayer::WebXRWebGLLayer(const WebXRSession&, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
    : m_context(WTFMove(context))
    , m_antialias(init.antialias)
    , m_ignoreDepthValues(init.ignoreDepthValues)
{
    m_framebuffer.object = WTF::switchOn(m_context,
        [](const RefPtr<WebGLRenderingContext>& context)
        {
            RefPtr<WebGLFramebuffer> frameBuffer = WebGLFramebuffer::create(*context);
            return frameBuffer;
        },
#if ENABLE(WEBGL2)
        [](const RefPtr<WebGL2RenderingContext>& context)
        {
            RefPtr<WebGLFramebuffer> frameBuffer = WebGLFramebuffer::create(*context);
            return frameBuffer;
        },
#endif
        [](WTF::Monostate) {
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    );
}

WebXRWebGLLayer::~WebXRWebGLLayer() = default;

bool WebXRWebGLLayer::antialias() const
{
    return m_antialias;
}

bool WebXRWebGLLayer::ignoreDepthValues() const
{
    return m_ignoreDepthValues;
}

const WebGLFramebuffer& WebXRWebGLLayer::framebuffer() const
{
    return *m_framebuffer.object;
}

unsigned WebXRWebGLLayer::framebufferWidth() const
{
    return m_framebuffer.width;
}

unsigned WebXRWebGLLayer::framebufferHeight() const
{
    return m_framebuffer.height;
}

RefPtr<WebXRViewport> WebXRWebGLLayer::getViewport(const WebXRView&)
{
    return { };
}

double WebXRWebGLLayer::getNativeFramebufferScaleFactor(const WebXRSession&)
{
    return 1.0;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
