/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021-2023 Apple, Inc. All rights reserved.
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

#include "GraphicsContextGL.h"
#include "GraphicsTypesGL.h"
#include "PlatformXR.h"
#include "WebXRLayer.h"
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class IntSize;
class WebGLFramebuffer;
class WebGLRenderingContextBase;
struct XRWebGLLayerInit;

class WebXROpaqueFramebuffer {
public:
    struct Attributes {
        bool alpha { true };
        bool antialias { true };
        bool depth { true };
        bool stencil { false };
    };

    static std::unique_ptr<WebXROpaqueFramebuffer> create(PlatformXR::LayerHandle, WebGLRenderingContextBase&, Attributes&&, IntSize);
    ~WebXROpaqueFramebuffer();

    PlatformXR::LayerHandle handle() const { return m_handle; }
    const WebGLFramebuffer& framebuffer() const { return m_framebuffer.get(); }
    GCGLint width() const { return m_framebufferSize.width(); }
    GCGLint height() const { return m_framebufferSize.height(); }

    void startFrame(const PlatformXR::FrameData::LayerData&);
    void endFrame();

private:
    WebXROpaqueFramebuffer(PlatformXR::LayerHandle, Ref<WebGLFramebuffer>&&, WebGLRenderingContextBase&, Attributes&&, IntSize);

    bool setupFramebuffer();
    PlatformGLObject allocateRenderbufferStorage(GraphicsContextGL&, GCGLsizei, GCGLenum, IntSize);
    PlatformGLObject allocateColorStorage(GraphicsContextGL&, GCGLsizei, IntSize);
    PlatformGLObject allocateDepthStencilStorage(GraphicsContextGL&, GCGLsizei, IntSize);
    void bindColorBuffer(GraphicsContextGL&, PlatformGLObject);
    void bindDepthStencilBuffer(GraphicsContextGL&, PlatformGLObject);

    PlatformXR::LayerHandle m_handle;
    Ref<WebGLFramebuffer> m_framebuffer;
    WebGLRenderingContextBase& m_context;
    Attributes m_attributes;
    IntSize m_framebufferSize;
    GCGLOwnedRenderbuffer m_depthStencilBuffer;
    GCGLOwnedRenderbuffer m_multisampleColorBuffer;
    GCGLOwnedRenderbuffer m_multisampleDepthStencilBuffer;
    GCGLOwnedFramebuffer m_resolvedFBO;
#if PLATFORM(COCOA)
    GCGLOwnedTexture m_colorTexture;
    GCEGLImage m_colorImage { };
    GCEGLImage m_depthStencilImage { };
    GraphicsContextGL::ExternalEGLSyncEvent m_completionSyncEvent;
#else
    PlatformGLObject m_colorTexture;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
