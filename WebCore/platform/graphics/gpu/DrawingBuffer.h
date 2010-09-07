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

#include "GraphicsLayer.h"
#include "IntSize.h"

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class SharedGraphicsContext3D;

struct DrawingBufferInternal;

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can publish its rendering
// results to a PlatformLayer for compositing.
class DrawingBuffer : public Noncopyable {
public:
    static PassOwnPtr<DrawingBuffer> create(SharedGraphicsContext3D*, const IntSize&);
    ~DrawingBuffer();

    void reset(const IntSize&);
    void bind();
    IntSize size() const { return m_size; }

#if USE(ACCELERATED_COMPOSITING)
    PlatformLayer* platformLayer();
    void publishToPlatformLayer();
#endif

    unsigned getRenderingResultsAsTexture();

    class WillPublishCallback : public Noncopyable {
    public:
        virtual void willPublish() = 0;
    };

    void setWillPublishCallback(PassOwnPtr<WillPublishCallback>);
private:
    DrawingBuffer(SharedGraphicsContext3D*, const IntSize&, unsigned framebuffer);

    SharedGraphicsContext3D* m_context;
    IntSize m_size;
    unsigned m_framebuffer;

    OwnPtr<WillPublishCallback> m_callback;
    OwnPtr<DrawingBufferInternal> m_internal;
};

} // namespace WebCore

#endif // DrawingBuffer_h
