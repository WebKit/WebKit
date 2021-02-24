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

#include "HTMLCanvasElement.h"
#include "IntSize.h"
#include "OffscreenCanvas.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderingContext.h"
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#include "WebGLRenderingContextBase.h"
#include "WebXRFrame.h"
#include "WebXRSession.h"
#include "WebXRView.h"
#include "WebXRViewport.h"
#include "XRWebGLLayerInit.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRWebGLLayer);

// https://immersive-web.github.io/webxr/#dom-xrwebgllayer-xrwebgllayer
ExceptionOr<Ref<WebXRWebGLLayer>> WebXRWebGLLayer::create(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
{
    // 1. Let layer be a new XRWebGLLayer
    // 2. If session’s ended value is true, throw an InvalidStateError and abort these steps.
    if (session->ended())
        return Exception { InvalidStateError };

    // 3. If context is lost, throw an InvalidStateError and abort these steps.
    // 4. If session is an immersive session and context’s XR compatible boolean is false, throw
    //    an InvalidStateError and abort these steps.
    return WTF::switchOn(context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) -> ExceptionOr<Ref<WebXRWebGLLayer>>
        {
            if (baseContext->isContextLost())
                return Exception { InvalidStateError };

            auto mode = session->mode();
            if ((mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr) && !baseContext->isXRCompatible())
                return Exception { InvalidStateError };


            // 5. Initialize layer’s context to context. (see constructor)
            // 6. Initialize layer’s session to session. (see constructor)
            // 7. Initialize layer’s ignoreDepthValues as follows. (see constructor)
            // 8. Initialize layer’s composition disabled boolean as follows. (see constructor)
            // 9. (see constructor except for the resources initialization step which is handled in the if block below)
            auto layer = adoptRef(*new WebXRWebGLLayer(WTFMove(session), WTFMove(context), init));

            if (layer->m_isCompositionEnabled) {
                // 9.4. Allocate and initialize resources compatible with session’s XR device, including GPU accessible memory buffers,
                //      as required to support the compositing of layer.
                // 9.5. If layer’s resources were unable to be created for any reason, throw an OperationError and abort these steps.
                // TODO: Initialize layer's resources or issue an OperationError.
            }

            // 10. Return layer.
            return layer;
        },
        [](WTF::Monostate) {
            ASSERT_NOT_REACHED();
            return Exception { InvalidStateError };
        }
    );
}

WebXRWebGLLayer::WebXRWebGLLayer(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
    : WebXRLayer(session->scriptExecutionContext())
    , m_session(WTFMove(session))
    , m_context(WTFMove(context))
    , m_leftViewportData({ WebXRViewport::create({ }) })
    , m_rightViewportData({ WebXRViewport::create({ }) })
{
    // 7. Initialize layer’s ignoreDepthValues as follows:
    //   7.1 If layerInit’s ignoreDepthValues value is false and the XR Compositor will make use of depth values,
    //       Initialize layer’s ignoreDepthValues to false.
    //   7.2. Else Initialize layer’s ignoreDepthValues to true
    // TODO: ask XR compositor for depth value usages
    m_ignoreDepthValues = init.ignoreDepthValues;

    // 8. Initialize layer's composition disabled boolean as follows:
    //  If session is an inline session -> Initialize layer's composition disabled to true
    //  Otherwise -> Initialize layer's composition disabled boolean to false
    m_isCompositionEnabled = m_session->mode() != XRSessionMode::Inline;

    // 9. If layer’s composition disabled boolean is false:
    if (m_isCompositionEnabled) {
        //  1. Initialize layer’s antialias to layerInit’s antialias value.
        m_antialias = init.antialias;

        //  2. Let framebufferSize be the recommended WebGL framebuffer resolution multiplied by layerInit's framebufferScaleFactor.
        IntSize recommendedSize = m_session->recommendedWebGLFramebufferResolution();
        m_framebuffer.width = recommendedSize.width() * init.framebufferScaleFactor;
        m_framebuffer.height = recommendedSize.height() * init.framebufferScaleFactor;

        //  3. Initialize layer’s framebuffer to a new opaque framebuffer with the dimensions framebufferSize
        //       created with context, session initialized to session, and layerInit’s depth, stencil, and alpha values.
        // For steps 4 & 5 see the create() method as resources initialization is handled outside the constructor as it might trigger
        // an OperationError.
        // FIXME: create a proper opaque framebuffer.
        m_framebuffer.object = WTF::switchOn(m_context,
            [](const RefPtr<WebGLRenderingContextBase>& baseContext)
            {
                return WebGLFramebuffer::create(*baseContext);
            }
        );
    } else {
        // 1. Initialize layer’s antialias to layer’s context's actual context parameters antialias value.
        m_antialias = WTF::switchOn(m_context,
            [](const RefPtr<WebGLRenderingContextBase>& context)
            {
                if (auto attributes = context->getContextAttributes())
                    return attributes.value().antialias;
                return false;
            }
        );
        // 2. Initialize layer’s framebuffer to null.
        m_framebuffer.object = nullptr;
    }

    auto canvasElement = canvas();
    if (canvasElement)
        canvasElement->addObserver(*this);
}

WebXRWebGLLayer::~WebXRWebGLLayer()
{
    auto canvasElement = canvas();
    if (canvasElement)
        canvasElement->removeObserver(*this);
}

bool WebXRWebGLLayer::antialias() const
{
    return m_antialias;
}

bool WebXRWebGLLayer::ignoreDepthValues() const
{
    return m_ignoreDepthValues;
}

WebGLFramebuffer* WebXRWebGLLayer::framebuffer() const
{
    return m_framebuffer.object.get();
}

unsigned WebXRWebGLLayer::framebufferWidth() const
{
    if (m_framebuffer.object)
        return m_framebuffer.width;
    return WTF::switchOn(m_context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) {
            return baseContext->drawingBufferWidth();
        });
}

unsigned WebXRWebGLLayer::framebufferHeight() const
{
    if (m_framebuffer.object)
        return m_framebuffer.height;
    return WTF::switchOn(m_context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) {
            return baseContext->drawingBufferHeight();
        });
}

// https://immersive-web.github.io/webxr/#dom-xrwebgllayer-getviewport
ExceptionOr<RefPtr<WebXRViewport>> WebXRWebGLLayer::getViewport(WebXRView& view)
{
    // 1. Let session be view’s session.
    // 2. Let frame be session’s animation frame.
    // 3. If session is not equal to layer’s session, throw an InvalidStateError and abort these steps.
    if (&view.frame().session() != m_session.ptr())
        return Exception { InvalidStateError };

    // 4. If frame’s active boolean is false, throw an InvalidStateError and abort these steps.
    // 5. If view’s frame is not equal to frame, throw an InvalidStateError and abort these steps.
    if (!view.frame().isActive() || !view.frame().isAnimationFrame())
        return Exception { InvalidStateError }; 

    auto& viewportData = view.eye() == XREye::Right ? m_rightViewportData : m_leftViewportData;

    // 6. If the viewport modifiable flag is true and view’s requested viewport scale is not equal to current viewport scale:
    //   6.1 Set current viewport scale to requested viewport scale.
    //   6.2 Compute the scaled viewport.
    if (view.isViewportModifiable() && view.requestedViewportScale() != viewportData.currentScale) {
        viewportData.currentScale = view.requestedViewportScale();
        m_viewportsDirty = true;
    }

    // 7. Set the view’s viewport modifiable flag to false.
    view.setViewportModifiable(false);

    if (m_viewportsDirty)
        computeViewports();

    // 8. Let viewport be the XRViewport from the list of viewport objects associated with view.
    // 9. Return viewport.
    return RefPtr<WebXRViewport>(viewportData.viewport.copyRef());
}

double WebXRWebGLLayer::getNativeFramebufferScaleFactor(const WebXRSession& session)
{
    if (session.ended())
        return 0.0;

    IntSize nativeSize = session.nativeWebGLFramebufferResolution();
    IntSize recommendedSize = session.recommendedWebGLFramebufferResolution();

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!recommendedSize.isZero());
    return (nativeSize / recommendedSize).width();
}

HTMLCanvasElement* WebXRWebGLLayer::canvas() const
{
    return WTF::switchOn(m_context, [](const RefPtr<WebGLRenderingContextBase>& baseContext) {
        auto canvas = baseContext->canvas();
        return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& canvas) {
            return canvas.get();
        }, [](const RefPtr<OffscreenCanvas>) {
            ASSERT_NOT_REACHED("baseLayer of a WebXRWebGLLayer must be an HTMLCanvasElement");
            return nullptr;
        });
    });
}

// https://immersive-web.github.io/webxr/#xrview-obtain-a-scaled-viewport
void WebXRWebGLLayer::computeViewports()
{
    auto roundDown = [](double value) -> int {
        // Round down to integer value and ensure that the value is not zero.
        return std::max(1, static_cast<int>(std::floor(value)));
    };

    auto width = framebufferWidth();
    auto height = framebufferHeight();

    if (m_session->mode() == XRSessionMode::ImmersiveVr) {
        // Update left viewport
        auto scale = m_leftViewportData.currentScale;
        m_leftViewportData.viewport->updateViewport(IntRect(0, 0, roundDown(width * 0.5 * scale), roundDown(height * scale)));

        // Update right viewport
        scale = m_rightViewportData.currentScale;
        m_rightViewportData.viewport->updateViewport(IntRect(width * 0.5, 0, roundDown(width * 0.5 * scale), roundDown(height * scale)));
    } else {
        // We reuse m_leftViewport for XREye::None.
        m_leftViewportData.viewport->updateViewport(IntRect(0, 0, width, height));
    }

    m_viewportsDirty = false;
}

void WebXRWebGLLayer::canvasResized(CanvasBase&)
{
    m_viewportsDirty = true;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
