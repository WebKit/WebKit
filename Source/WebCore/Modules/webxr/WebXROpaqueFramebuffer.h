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
#include <wtf/Vector.h>

namespace WebCore {

class IntSize;
class WebGLFramebuffer;
class WebGLRenderingContextBase;
struct XRWebGLLayerInit;

struct WebXRExternalRenderbuffer {
    GCGLOwnedRenderbuffer renderBufferObject;
    GCGLOwnedExternalImage image;

    explicit operator bool() const { return !!image; }

    void destroyImage(GraphicsContextGL&);
    void release(GraphicsContextGL&);
    void leakObject();
};

template<typename T>
struct WebXRAttachmentSet {
    T colorBuffer;
    T depthStencilBuffer;

    operator bool() const
    {
        return !!colorBuffer; // Need colorBuffer at the minimum!
    }

    void release(GraphicsContextGL& gl)
    {
        colorBuffer.release(gl);
        depthStencilBuffer.release(gl);
    }

    void leakObject()
    {
        colorBuffer.leakObject();
        depthStencilBuffer.leakObject();
    }
};

using WebXRAttachments = WebXRAttachmentSet<GCGLOwnedRenderbuffer>;
using WebXRExternalAttachments = WebXRAttachmentSet<WebXRExternalRenderbuffer>;

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

    bool supportsDynamicViewportScaling() const;

    PlatformXR::LayerHandle handle() const { return m_handle; }
    const WebGLFramebuffer& framebuffer() const { return m_drawFramebuffer.get(); }
    // Return the size of the framebuffer is Screen Space
    IntSize drawFramebufferSize() const;
    // Return the viewport for eye in Screen Space
    IntRect drawViewport(PlatformXR::Eye) const;

    void startFrame(const PlatformXR::FrameData::LayerData&);
    void endFrame();
    bool usesLayeredMode() const;

#if PLATFORM(COCOA)
    void releaseAllDisplayAttachments();
#endif

private:
    WebXROpaqueFramebuffer(PlatformXR::LayerHandle, Ref<WebGLFramebuffer>&&, WebGLRenderingContextBase&, Attributes&&, IntSize);

#if PLATFORM(COCOA)
    bool setupFramebuffer(GraphicsContextGL&, const PlatformXR::FrameData::LayerSetupData&);
    const std::array<WebXRExternalAttachments, 2>* reusableDisplayAttachments(const PlatformXR::FrameData::ExternalTextureData&) const;
    void bindCompositorTexturesForDisplay(GraphicsContextGL&, const PlatformXR::FrameData::LayerData&);
    const std::array<WebXRExternalAttachments, 2>* reusableDisplayAttachmentsAtIndex(size_t);
    void releaseDisplayAttachmentsAtIndex(size_t);
#endif
    void allocateRenderbufferStorage(GraphicsContextGL&, GCGLOwnedRenderbuffer&, GCGLsizei, GCGLenum, IntSize);
    void allocateAttachments(GraphicsContextGL&, WebXRAttachments&, GCGLsizei, IntSize);
    void bindAttachments(GraphicsContextGL&, WebXRAttachments&);
#if PLATFORM(COCOA)
    void bindResolveAttachments(GraphicsContextGL&, WebXRAttachments&);
#endif
    void resolveMSAAFramebuffer(GraphicsContextGL&);
    void blitShared(GraphicsContextGL&);
    void blitSharedToLayered(GraphicsContextGL&);
    IntRect calculateViewportShared(PlatformXR::Eye, bool, const IntRect&, const IntRect&);

    PlatformXR::LayerHandle m_handle;
    Ref<WebGLFramebuffer> m_drawFramebuffer;
    WebGLRenderingContextBase& m_context;
    Attributes m_attributes;
    PlatformXR::Layout m_displayLayout = PlatformXR::Layout::Shared;
    IntSize m_framebufferSize; // Physical Space
#if PLATFORM(COCOA)
    IntRect m_leftViewport; // Screen Space
    IntRect m_rightViewport; // Screen Space
    IntSize m_leftPhysicalSize; // Physical Space
    IntSize m_rightPhysicalSize; // Physical Space
#endif
    WebXRAttachments m_drawAttachments;
    WebXRAttachments m_resolveAttachments;
    GCGLOwnedFramebuffer m_displayFBO;
    GCGLOwnedFramebuffer m_resolvedFBO;
#if PLATFORM(COCOA)
    Vector<std::array<WebXRExternalAttachments, 2>> m_displayAttachmentsSets;
    size_t m_currentDisplayAttachmentIndex { 0 };
    MachSendRight m_completionSyncEvent;
    uint64_t m_renderingFrameIndex { ~0u };
    bool m_usingFoveation { false };
#else
    PlatformGLObject m_colorTexture;
#endif
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
