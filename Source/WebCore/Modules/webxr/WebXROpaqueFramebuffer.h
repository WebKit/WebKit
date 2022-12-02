/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
 * Copyright (C) 2021 Apple, Inc. All rights reserved.
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
    GCGLOwnedRenderbuffer m_depthStencilBuffer;
    GCGLOwnedRenderbuffer m_stencilBuffer;
    GCGLOwnedRenderbuffer m_multisampleColorBuffer;
    GCGLOwnedFramebuffer m_resolvedFBO;
    GCGLint m_sampleCount { 0 };
#if USE(IOSURFACE_FOR_XR_LAYER_DATA)
    GCGLOwnedTexture m_opaqueTexture;
    void* m_ioSurfaceTextureHandle { nullptr };
    bool m_ioSurfaceTextureHandleIsShared { false };
#else
    PlatformGLObject m_opaqueTexture;
#endif
#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    RetainPtr<id> m_completionEvent { nullptr };
    uint64_t m_renderingFrameIndex { 0 };
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
