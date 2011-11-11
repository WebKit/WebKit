/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DrawingBuffer_h
#define DrawingBuffer_h

#include "GraphicsContext3D.h"
#include "GraphicsLayer.h"
#include "GraphicsTypes3D.h"
#include "IntSize.h"

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {
class CanvasRenderingContext;
class GraphicsContext3D;
class ImageData;
#if PLATFORM(CHROMIUM)
class WebGLLayerChromium;
#endif

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can publish its rendering
// results to a PlatformLayer for compositing.
class DrawingBuffer : public RefCounted<DrawingBuffer> {
public:
    static PassRefPtr<DrawingBuffer> create(GraphicsContext3D*, const IntSize&, bool);
    friend class GraphicsContext3D;

    ~DrawingBuffer();

    void clearFramebuffer();

    // Returns true if the buffer was successfully resized.
    bool reset(const IntSize&);
    void bind();
    IntSize size() const { return m_size; }
    Platform3DObject colorBuffer() const { return m_colorBuffer; }

    // Clear all resources from this object, as well as context. Called when context is destroyed
    // to prevent invalid accesses to the resources.
    void clear();

    // Create the depth/stencil and multisample buffers, if needed.
    void createSecondaryBuffers();
    
    void resizeDepthStencil(int sampleCount);

    // Copies the multisample color buffer to the normal color buffer and leaves m_fbo bound
    void commit(long x = 0, long y = 0, long width = -1, long height = -1);

    // commit should copy the full multisample buffer, and not respect the
    // current scissor bounds. Track the state of the scissor test so that it
    // can be disabled during calls to commit.
    void setScissorEnabled(bool scissorEnabled) { m_scissorEnabled = scissorEnabled; }

    bool multisample() const;

    Platform3DObject platformColorBuffer() const;
    Platform3DObject framebuffer() const;

    PassRefPtr<ImageData> paintRenderingResultsToImageData();

    // Immediately releases ownership of all resources. Call upon loss of the
    // graphics context to prevent freeing invalid resources.
    void discardResources();

#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer();
    void publishToPlatformLayer();
    void paintCompositedResultsToCanvas(CanvasRenderingContext*);
#endif

    PassRefPtr<GraphicsContext3D> graphicsContext3D() const { return m_context; }

private:
    DrawingBuffer(GraphicsContext3D*, const IntSize&, bool multisampleExtensionSupported,
                  bool packedDepthStencilExtensionSupported, bool separateBackingTexture);

    void initialize(const IntSize&);

    bool m_separateBackingTexture;
    bool m_scissorEnabled;

    RefPtr<GraphicsContext3D> m_context;
    IntSize m_size;
    bool m_multisampleExtensionSupported;
    bool m_packedDepthStencilExtensionSupported;
    Platform3DObject m_fbo;
    Platform3DObject m_colorBuffer;
    Platform3DObject m_backingColorBuffer;

    // This is used when we have OES_packed_depth_stencil.
    Platform3DObject m_depthStencilBuffer;

    // These are used when we don't.
    Platform3DObject m_depthBuffer;
    Platform3DObject m_stencilBuffer;

    // For multisampling
    Platform3DObject m_multisampleFBO;
    Platform3DObject m_multisampleColorBuffer;

#if PLATFORM(CHROMIUM)
    RefPtr<WebGLLayerChromium> m_platformLayer;
#endif

#if PLATFORM(MAC)
    RetainPtr<WebGLLayer> m_platformLayer;
#endif
};

} // namespace WebCore

#endif // DrawingBuffer_h
