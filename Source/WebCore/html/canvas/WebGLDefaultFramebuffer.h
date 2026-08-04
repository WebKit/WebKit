/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBGL)

#include "WebGLRenderingContextBase.h"
#include "WebGLUtilities.h"
#include <optional>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class IntRect;

// Implementation for the WebGL context default framebuffer.
class WebGLDefaultFramebuffer {
    WTF_MAKE_TZONE_ALLOCATED(WebGLDefaultFramebuffer);
    WTF_MAKE_NONCOPYABLE(WebGLDefaultFramebuffer);
public:
    // Creates the framebuffer with a 0x0 size. The caller must call reshape() once the
    // context is initialized to allocate and configure the attachments.
    static std::unique_ptr<WebGLDefaultFramebuffer> create(WebGLRenderingContextBase&);
    ~WebGLDefaultFramebuffer();

    PlatformGLObject object() const { return m_fbo; }

    // Resolves/blits the rendered color into the result FBO (id 0). No-op for the
    // direct-rendering case.
    void resolveColorIntoResult(std::optional<IntRect> = std::nullopt);

    // For default-FB reads (readPixels, copyTexImage, etc.): when antialias is in
    // effect, resolves the requested rect into the result FBO and binds the GL read
    // framebuffer to 0 so the read sees the resolved color.
    [[nodiscard]] std::optional<ScopedWebGLRestoreFramebuffer> prepareForReadWhenBound(std::optional<IntRect> = std::nullopt);

    bool hasStencil() const
    {
        return m_depthStencilAttachment == GraphicsContextGL::STENCIL_ATTACHMENT
            || m_depthStencilAttachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT;
    }
    bool hasDepth() const
    {
        return m_depthStencilAttachment == GraphicsContextGL::DEPTH_ATTACHMENT
            || m_depthStencilAttachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT;
    }
    IntSize size() const { return m_size; }
    void reshape(IntSize);
    GCGLbitfield dirtyBuffers() const { return m_dirtyBuffers; }
    void NODELETE markBuffersClear(GCGLbitfield clearBuffers);
    void NODELETE markAllUnpreservedBuffersDirty();
    void NODELETE markAllBuffersDirty();

private:
    WebGLDefaultFramebuffer(WebGLRenderingContextBase&);

    WeakRef<WebGLRenderingContextBase> m_context;

    // m_fbo == 0 renders straight into the result FBO. When antialiasing or preserving
    // the drawing buffer m_fbo is an offscreen FBO created in the constructor; its
    // renderbuffers are created and attached lazily on the first reshape(). The absence
    // of a created renderbuffer is what signals that the FBO still needs configuring.
    PlatformGLObject m_fbo { 0 };
    PlatformGLObject m_colorBuffer { 0 };
    PlatformGLObject m_depthStencilBuffer { 0 };

    GCGLenum m_depthStencilFormat { 0 };
    GCGLenum m_depthStencilAttachment { 0 };

    IntSize m_size;
    GCGLbitfield m_unpreservedBuffers { 0 };
    GCGLbitfield m_dirtyBuffers { 0 };
};

}

#endif
