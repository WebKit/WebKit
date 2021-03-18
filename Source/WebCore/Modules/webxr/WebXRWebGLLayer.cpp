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

#include "ExtensionsGLOpenGLCommon.h"
#include "ExtensionsGLOpenGLES.h"
#include "GraphicsContextGL.h"
#include "HTMLCanvasElement.h"
#include "IntSize.h"
#include "OffscreenCanvas.h"
#include "TemporaryOpenGLSetting.h"
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

using GL = GraphicsContextGL;

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRWebGLLayer);

// Arbitrary value for minimum framebuffer scaling.
// Below this threshold the resulting framebuffer would be too small to see.
constexpr double MinFramebufferScalingFactor = 0.2;

static ExceptionOr<std::unique_ptr<WebXROpaqueFramebuffer>> createOpaqueFramebuffer(WebXRSession& session, WebGLRenderingContextBase& context, const XRWebGLLayerInit& init)
{
    auto device = session.device();
    if (!device)
        return Exception { OperationError, "Cannot create an XRWebGLLayer with an XRSession that has ended." };

    // 9.1. Initialize layer’s antialias to layerInit’s antialias value.
    // 9.2. Let framebufferSize be the recommended WebGL framebuffer resolution multiplied by layerInit's framebufferScaleFactor.

    float scaleFactor = std::clamp(init.framebufferScaleFactor, MinFramebufferScalingFactor, device->maxFramebufferScalingFactor());

    IntSize recommendedSize = session.recommendedWebGLFramebufferResolution();
    auto width = static_cast<uint32_t>(std::ceil(2 * recommendedSize.width() * scaleFactor));
    auto height = static_cast<uint32_t>(std::ceil(recommendedSize.height() * scaleFactor));

    // 9.3. Initialize layer’s framebuffer to a new opaque framebuffer with the dimensions framebufferSize
    //      created with context, session initialized to session, and layerInit’s depth, stencil, and alpha values.
    // 9.4. Allocate and initialize resources compatible with session’s XR device, including GPU accessible memory buffers,
    //      as required to support the compositing of layer.
    // 9.5. If layer’s resources were unable to be created for any reason, throw an OperationError and abort these steps.
    auto layerHandle = device->createLayerProjection(width, height, init.alpha);
    if (!layerHandle)
        return Exception { OperationError, "Unable to allocate XRWebGLLayer GPU resources."};

    WebXROpaqueFramebuffer::Attributes attributes {
        .alpha = init.alpha,
        .antialias = init.antialias,
        .depth = init.depth,
        .stencil = init.stencil
    };

    auto framebuffer = WebXROpaqueFramebuffer::create(*layerHandle, context, WTFMove(attributes), width, height);
    if (!framebuffer)
        return Exception { OperationError, "Unable to create a framebuffer." };
    
    return framebuffer;
}

// https://immersive-web.github.io/webxr/#dom-xrwebgllayer-xrwebgllayer
ExceptionOr<Ref<WebXRWebGLLayer>> WebXRWebGLLayer::create(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, const XRWebGLLayerInit& init)
{
    // 1. Let layer be a new XRWebGLLayer
    // 2. If session’s ended value is true, throw an InvalidStateError and abort these steps.
    if (session->ended())
        return Exception { InvalidStateError, "Cannot create an XRWebGLLayer with an XRSession that has ended." };

    // 3. If context is lost, throw an InvalidStateError and abort these steps.
    // 4. If session is an immersive session and context’s XR compatible boolean is false, throw
    //    an InvalidStateError and abort these steps.
    return WTF::switchOn(context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) -> ExceptionOr<Ref<WebXRWebGLLayer>>
        {
            if (baseContext->isContextLost())
                return Exception { InvalidStateError, "Cannot create an XRWebGLLayer with a lost WebGL context." };

            auto mode = session->mode();
            if ((mode == XRSessionMode::ImmersiveAr || mode == XRSessionMode::ImmersiveVr) && !baseContext->isXRCompatible())
                return Exception { InvalidStateError, "Cannot create an XRWebGLLayer with WebGL context not marked as XR compatible." };


            // 5. Initialize layer’s context to context. (see constructor)
            // 6. Initialize layer’s session to session. (see constructor)
            // 7. Initialize layer’s ignoreDepthValues as follows. (see constructor)
            //   7.1 If layerInit’s ignoreDepthValues value is false and the XR Compositor will make use of depth values,
            //       Initialize layer’s ignoreDepthValues to false.
            //   7.2. Else Initialize layer’s ignoreDepthValues to true
            // TODO: ask XR compositor for depth value usages support
            bool ignoreDepthValues = true;
            // 8. Initialize layer’s composition disabled boolean as follows.
            //    If session is an inline session -> Initialize layer's composition disabled to true
            //    Otherwise -> Initialize layer's composition disabled boolean to false
            bool isCompositionEnabled = session->mode() != XRSessionMode::Inline;
            bool antialias = false;
            std::unique_ptr<WebXROpaqueFramebuffer> framebuffer;

            // 9. If layer's composition enabled boolean is true: 
            if (isCompositionEnabled) {
                auto createResult = createOpaqueFramebuffer(session.get(), *baseContext, init);
                if (createResult.hasException())
                    return createResult.releaseException();
                framebuffer = createResult.releaseReturnValue();
            } else {
                // Otherwise
                // 9.1. Initialize layer’s antialias to layer’s context's actual context parameters antialias value.
                if (auto attributes = baseContext->getContextAttributes())
                    antialias = attributes->antialias;
                // 9.2 Initialize layer's framebuffer to null.
            }

            // 10. Return layer.
            return adoptRef(*new WebXRWebGLLayer(WTFMove(session), WTFMove(context), WTFMove(framebuffer), antialias, ignoreDepthValues, isCompositionEnabled));
        },
        [](WTF::Monostate) {
            ASSERT_NOT_REACHED();
            return Exception { InvalidStateError };
        }
    );
}

WebXRWebGLLayer::WebXRWebGLLayer(Ref<WebXRSession>&& session, WebXRRenderingContext&& context, std::unique_ptr<WebXROpaqueFramebuffer>&& framebuffer,
    bool antialias, bool ignoreDepthValues, bool isCompositionEnabled)
    : WebXRLayer(session->scriptExecutionContext())
    , m_session(WTFMove(session))
    , m_context(WTFMove(context))
    , m_leftViewportData({ WebXRViewport::create({ }) })
    , m_rightViewportData({ WebXRViewport::create({ }) })
    , m_framebuffer(WTFMove(framebuffer))
    , m_antialias(antialias)
    , m_ignoreDepthValues(ignoreDepthValues)
    , m_isCompositionEnabled(isCompositionEnabled)
{
}

WebXRWebGLLayer::~WebXRWebGLLayer()
{
    auto canvasElement = canvas();
    if (canvasElement)
        canvasElement->removeObserver(*this);
    if (m_framebuffer) {
        auto device = m_session->device();
        if (device)
            device->deleteLayer(m_framebuffer->handle());
    }
}

bool WebXRWebGLLayer::antialias() const
{
    return m_antialias;
}

bool WebXRWebGLLayer::ignoreDepthValues() const
{
    return m_ignoreDepthValues;
}

const WebGLFramebuffer* WebXRWebGLLayer::framebuffer() const
{
    return m_framebuffer ? &m_framebuffer->framebuffer() : nullptr;
}

unsigned WebXRWebGLLayer::framebufferWidth() const
{
    if (m_framebuffer)
        return m_framebuffer->width();
    return WTF::switchOn(m_context,
        [&](const RefPtr<WebGLRenderingContextBase>& baseContext) {
            return baseContext->drawingBufferWidth();
        });
}

unsigned WebXRWebGLLayer::framebufferHeight() const
{
    if (m_framebuffer)
        return m_framebuffer->height();
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


void WebXRWebGLLayer::startFrame(const PlatformXR::Device::FrameData& data)
{
    ASSERT(m_framebuffer);

    auto it = data.layers.find(m_framebuffer->handle());
    if (it == data.layers.end()) {
        // For some reason the device didn't provide a texture for this frame.
        // The frame is ignored and the device can recover the texture in future frames;
        return;
    }

    m_framebuffer->startFrame(it->value);
}

PlatformXR::Device::Layer WebXRWebGLLayer::endFrame()
{
    ASSERT(m_framebuffer);
    m_framebuffer->endFrame();

    Vector<PlatformXR::Device::LayerView> views {
        { PlatformXR::Eye::Left, m_leftViewportData.viewport->rect() },
        { PlatformXR::Eye::Right, m_rightViewportData.viewport->rect() }
    };

    return PlatformXR::Device::Layer { .handle = m_framebuffer->handle(), .visible = true, .views = WTFMove(views) };
}

void WebXRWebGLLayer::canvasResized(CanvasBase&)
{
    m_viewportsDirty = true;
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

// WebXROpaqueFramebuffer

std::unique_ptr<WebXROpaqueFramebuffer> WebXROpaqueFramebuffer::create(PlatformXR::LayerHandle handle, WebGLRenderingContextBase& context, Attributes&& attributes, uint32_t width, uint32_t height)
{
    auto framebuffer = WebGLFramebuffer::createOpaque(context);
    auto opaque = std::unique_ptr<WebXROpaqueFramebuffer>(new WebXROpaqueFramebuffer(handle, WTFMove(framebuffer), context, WTFMove(attributes), width, height));
    if (!opaque->setupFramebuffer())
        return nullptr;
    return opaque;
}

WebXROpaqueFramebuffer::WebXROpaqueFramebuffer(PlatformXR::LayerHandle handle, Ref<WebGLFramebuffer>&& framebuffer, WebGLRenderingContextBase& context, Attributes&& attributes, uint32_t width, uint32_t height)
    : m_handle(handle)
    , m_framebuffer(WTFMove(framebuffer))
    , m_context(context)
    , m_attributes(WTFMove(attributes))
    , m_width(width)
    , m_height(height)
{
}

WebXROpaqueFramebuffer::~WebXROpaqueFramebuffer()
{
    if (auto gl = m_context.graphicsContextGL()) {
        if (m_stencilBuffer)
            gl->deleteRenderbuffer(m_stencilBuffer);
        if (m_depthStencilBuffer)
            gl->deleteRenderbuffer(m_depthStencilBuffer);
        if (m_multisampleColorBuffer)
            gl->deleteRenderbuffer(m_multisampleColorBuffer);
        if (m_resolvedFBO)
            gl->deleteFramebuffer(m_resolvedFBO);
        m_context.deleteFramebuffer(m_framebuffer.ptr());
    }
}

void WebXROpaqueFramebuffer::startFrame(const PlatformXR::Device::FrameData::LayerData& data)
{
    m_opaqueTexture = data.opaqueTexture;
    if (!m_context.graphicsContextGL())
        return;
    auto& gl = *m_context.graphicsContextGL();

    m_framebuffer->setOpaqueActive(true);

    GCGLint boundFBO { 0 };

    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, makeGCGLSpan(&boundFBO, 1));
    auto scopedFBOs = makeScopeExit([&gl, boundFBO]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
    });

    gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
    // https://immersive-web.github.io/webxr/#opaque-framebuffer
    // The buffers attached to an opaque framebuffer MUST be cleared to the values in the table below when first created,
    // or prior to the processing of each XR animation frame.
    std::array<const GCGLenum, 3> attachments = { GL::COLOR_ATTACHMENT0, GL::STENCIL_ATTACHMENT, GL::DEPTH_ATTACHMENT };
    gl.invalidateFramebuffer(GL::FRAMEBUFFER, makeGCGLSpan(attachments.data(), attachments.size()));

#if USE(OPENGL_ES)
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLES&>(gl.getExtensions());
    if (m_attributes.antialias && extensions.isImagination()) {
        extensions.framebufferTexture2DMultisampleIMG(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0, m_sampleCount);
        return;
    }
#endif

    if (!m_multisampleColorBuffer)
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);
}

void WebXROpaqueFramebuffer::endFrame()
{
    m_framebuffer->setOpaqueActive(false);

    if (!m_context.graphicsContextGL())
        return;
    auto& gl = *m_context.graphicsContextGL();

    if (m_multisampleColorBuffer) {
        TemporaryOpenGLSetting scopedScissor(GL::SCISSOR_TEST, 0);
        TemporaryOpenGLSetting scopedDither(GL::DITHER, 0);
        TemporaryOpenGLSetting scopedDepth(GL::DEPTH_TEST, 0);
        TemporaryOpenGLSetting scopedStencil(GL::STENCIL_TEST, 0);

        GCGLint boundReadFBO { 0 };
        GCGLint boundDrawFBO { 0 };
        gl.getIntegerv(GL::READ_FRAMEBUFFER_BINDING, makeGCGLSpan(&boundReadFBO, 1));
        gl.getIntegerv(GL::DRAW_FRAMEBUFFER_BINDING, makeGCGLSpan(&boundDrawFBO, 1));

        auto scopedFBOs = makeScopeExit([&gl, boundReadFBO, boundDrawFBO]() {
            gl.bindFramebuffer(GL::READ_FRAMEBUFFER, boundReadFBO);
            gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, boundDrawFBO);
        });

        gl.bindFramebuffer(GL::DRAW_FRAMEBUFFER, m_resolvedFBO);
        gl.framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, m_opaqueTexture, 0);

        // Resolve multisample framebuffer
        gl.bindFramebuffer(GL::READ_FRAMEBUFFER, m_framebuffer->object());
        gl.blitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL::COLOR_BUFFER_BIT, GL::LINEAR);
    }
    
    gl.flush();
}

bool WebXROpaqueFramebuffer::setupFramebuffer()
{
    if (!m_context.graphicsContextGL())
        return false;
    auto& gl = *m_context.graphicsContextGL();

    // Bind current FBOs when exiting the function
    GCGLint boundFBO { 0 };
    GCGLint boundRenderbuffer { 0 };
    gl.getIntegerv(GL::FRAMEBUFFER_BINDING, makeGCGLSpan(&boundFBO, 1));
    gl.getIntegerv(GL::RENDERBUFFER_BINDING, makeGCGLSpan(&boundRenderbuffer, 1));
    auto scopedFBO = makeScopeExit([&gl, boundFBO, boundRenderbuffer]() {
        gl.bindFramebuffer(GL::FRAMEBUFFER, boundFBO);
        gl.bindRenderbuffer(GL::RENDERBUFFER, boundRenderbuffer);
    });

    // Set up color, depth and stencil formats
    bool useDepthStencil = m_attributes.stencil || m_attributes.depth;
    auto colorFormat = m_attributes.alpha ? GraphicsContextGL::RGBA8 : GraphicsContextGL::RGB8;
#if USE(OPENGL_ES)
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLES&>(gl.getExtensions());
    bool supportsPackedDepthStencil = useDepthStencil && extensions.supports("GL_OES_packed_depth_stencil");
    auto depthFormat = supportsPackedDepthStencil ? GL::DEPTH24_STENCIL8 : GL::DEPTH_COMPONENT16;
    auto stencilFormat = GL::STENCIL_INDEX8;
#else
    auto& extensions = reinterpret_cast<ExtensionsGLOpenGLCommon&>(gl.getExtensions());
    bool supportsPackedDepthStencil = useDepthStencil && extensions.supports("GL_EXT_packed_depth_stencil");
    auto depthFormat = supportsPackedDepthStencil ? GL::DEPTH24_STENCIL8 : GL::DEPTH_COMPONENT;
    auto stencilFormat = GL::STENCIL_COMPONENT;
#endif

    // Set up recommended samples for WebXR.
    // FIXME: check if we can get recommended values from each device platform.
    if (m_attributes.antialias) {
        GCGLint maxSampleCount;
        gl.getIntegerv(ExtensionsGL::MAX_SAMPLES, makeGCGLSpan(&maxSampleCount, 1));
        // Using more than 4 samples might be overhead.
        m_sampleCount = std::min(4, maxSampleCount);
    }

#if USE(OPENGL_ES)
    // Use multisampled_render_to_texture extension if available.
    if (m_attributes.antialias && extensions.isImagination()) {
        // framebufferTexture2DMultisampleIMG is set up in startFrame call.
        if (!useDepthStencil)
            return true;
        
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        m_depthStencilBuffer = gl.createRenderbuffer();
        if (supportsPackedDepthStencil) {
            gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
            extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
            if (m_attributes.stencil)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            if (m_attributes.depth)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
        } else {
            if (m_attributes.stencil) {
                m_stencilBuffer = gl.createRenderbuffer();
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, stencilFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
            }
            if (m_attributes.depth) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                extensions.renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            }
        }
        return true;
    }
#endif // USE(OPENGL_ES)

    if (m_attributes.antialias && m_context.isWebGL2()) {
        // Use an extra FBO for multisample if multisampled_render_to_texture is not supported.
        m_resolvedFBO = gl.createFramebuffer();
        m_multisampleColorBuffer = gl.createRenderbuffer();
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        gl.bindRenderbuffer(GL::RENDERBUFFER, m_multisampleColorBuffer);
        gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, colorFormat, m_width, m_height);
        gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, m_multisampleColorBuffer);
        if (useDepthStencil) {
            m_depthStencilBuffer = gl.createRenderbuffer();
            if (supportsPackedDepthStencil) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                if (m_attributes.stencil)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
                if (m_attributes.depth)
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            } else {
                if (m_attributes.stencil) {
                    m_stencilBuffer = gl.createRenderbuffer();
                    gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                    gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, stencilFormat, m_width, m_height);
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
                }
                if (m_attributes.depth) {
                    gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                    gl.renderbufferStorageMultisample(GL::RENDERBUFFER, m_sampleCount, depthFormat, m_width, m_height);
                    gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
                }
            }
        }
        return gl.checkFramebufferStatus(GL::FRAMEBUFFER) == GL::FRAMEBUFFER_COMPLETE;
    }
    if (useDepthStencil) {
        gl.bindFramebuffer(GL::FRAMEBUFFER, m_framebuffer->object());
        m_depthStencilBuffer = gl.createRenderbuffer();
        if (supportsPackedDepthStencil) {
            gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
            gl.renderbufferStorage(GL::RENDERBUFFER, depthFormat, m_width, m_height);
            if (m_attributes.stencil)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            if (m_attributes.depth)
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
        } else {
            if (m_attributes.stencil) {
                m_stencilBuffer = gl.createRenderbuffer();
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_stencilBuffer);
                gl.renderbufferStorage(GL::RENDERBUFFER, stencilFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::STENCIL_ATTACHMENT, GL::RENDERBUFFER, m_stencilBuffer);
            }
            if (m_attributes.depth) {
                gl.bindRenderbuffer(GL::RENDERBUFFER, m_depthStencilBuffer);
                gl.renderbufferStorage(GL::RENDERBUFFER, depthFormat, m_width, m_height);
                gl.framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_ATTACHMENT, GL::RENDERBUFFER, m_depthStencilBuffer);
            }
        }
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
