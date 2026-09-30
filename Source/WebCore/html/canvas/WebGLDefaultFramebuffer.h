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
    // Creates the framebuffer with a 0x0 size. The caller must call setSize() and ensureSize()
    // once the context is initialized to allocate and configure the attachments.
    static std::unique_ptr<WebGLDefaultFramebuffer> create(WebGLRenderingContextBase&);
    ~WebGLDefaultFramebuffer();

    // Deletes the GraphicsContextGL objects. The object names are meaningful only in the
    // GraphicsContextGL instance that created them, so the caller must call this while that
    // instance is still the GL context of the passed in WebGL context. Nothing is deleted for a
    // lost context, which has already lost its objects. Must be called before destruction.
    void destroy(WebGLRenderingContextBase&);

    PlatformGLObject object() const { return m_fbo; }

    // Resolves/blits the rendered color into the result FBO (id 0) so that the WebGL
    // implementation can read the canvas contents from it. No-op for the direct-rendering
    // case, where the default framebuffer is the result FBO.
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

    // The answers to the queries about the default framebuffer, such as DEPTH_BITS or SAMPLES.
    // They depend only on the formats chosen at creation, not on whether or how the storage is
    // allocated, so they are answered without querying the GraphicsContextGL.
    GCGLint depthBits() const
    {
        if (!hasDepth())
            return 0;
        return m_depthStencilFormat == GraphicsContextGL::DEPTH_COMPONENT16 ? 16 : 24;
    }
    GCGLint stencilBits() const { return hasStencil() ? 8 : 0; }
    GCGLsizei sampleCount() const { return m_sampleCount; }

    // Sets the size of the drawing buffer, reported by size(). The storage is reallocated to the
    // new size and cleared in the next ensureSize(). Resizing the canvas with separate width and
    // height assignments thus does not allocate the storage for the intermediate sizes.
    void setSize(IntSize);
    // Reallocates the storage if the size was set since the last reallocation. Must be called
    // before the storage is used. Returns false if the storage could not be allocated. The caller
    // must then lose the context, as the default framebuffer is unusable.
    [[nodiscard]] bool ensureSize();
    // Returns true if the size set since the last reallocation needs less storage than the
    // current storage. Then the storage can be reallocated right away to release memory, as the
    // reallocation does not allocate more than is already allocated.
    bool pendingSizeShrinksStorage() const { return m_needsReshape && m_size.unclampedArea() < m_storageSize.unclampedArea(); }
    GCGLbitfield dirtyBuffers() const { return m_dirtyBuffers; }
    void NODELETE markBuffersClear(GCGLbitfield clearBuffers);
    void NODELETE markAllUnpreservedBuffersDirty();
    void NODELETE markAllBuffersDirty();

    // DRAW_BUFFER0 and READ_BUFFER of the default framebuffer, which the application can set to
    // BACK or NONE only. BACK is simulated with COLOR_ATTACHMENT0, so the state is tracked here
    // instead of being queried from the driver. The default framebuffer must be bound for drawing
    // or reading, respectively, when setting the state.
    void drawBuffers(GCGLenum);
    void readBuffer(GCGLenum);
    bool drawBufferIsNone() const { return m_drawBufferIsNone; }
    bool readBufferIsNone() const { return m_readBufferIsNone; }

private:
    WebGLDefaultFramebuffer(WebGLRenderingContextBase&);
    [[nodiscard]] bool reshape();

    WeakRef<WebGLRenderingContextBase> m_context;

    // m_fbo == 0 renders straight into the result FBO. When antialiasing or preserving
    // the drawing buffer m_fbo is an offscreen FBO created in the constructor; its
    // renderbuffers are created and attached lazily on the first ensureSize(). The absence
    // of a created renderbuffer is what signals that the FBO still needs configuring.
    PlatformGLObject m_fbo { 0 };
    PlatformGLObject m_colorBuffer { 0 };
    PlatformGLObject m_depthStencilBuffer { 0 };

    GCGLenum m_depthStencilFormat { 0 };
    GCGLenum m_depthStencilAttachment { 0 };
    GCGLsizei m_sampleCount { 0 };

    IntSize m_size;
    IntSize m_storageSize; // The size of the storage, as of the last reallocation.
    bool m_needsReshape { false };
    GCGLbitfield m_unpreservedBuffers { 0 };
    GCGLbitfield m_dirtyBuffers { 0 };
    bool m_drawBufferIsNone { false }; // Of m_fbo state.
    bool m_readBufferIsNone { false }; // Of m_fbo state.
};

}

#endif
