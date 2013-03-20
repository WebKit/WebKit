/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "GraphicsContext3D.h"
#include "IntSize.h"
#include "SkDeferredCanvas.h"
#include <public/WebExternalTextureLayer.h>
#include <public/WebExternalTextureLayerClient.h>
#include <wtf/DoublyLinkedList.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class Canvas2DLayerBridge : public WebKit::WebExternalTextureLayerClient, public SkDeferredCanvas::NotificationClient, public DoublyLinkedListNode<Canvas2DLayerBridge> {
    WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);
public:
    enum ThreadMode {
        SingleThread,
        Threaded
    };

    static PassOwnPtr<Canvas2DLayerBridge> create(PassRefPtr<GraphicsContext3D> context, const IntSize& size, ThreadMode threading, unsigned textureId)
    {
        return adoptPtr(new Canvas2DLayerBridge(context, size, threading, textureId));
    }

    virtual ~Canvas2DLayerBridge();

    // WebKit::WebExternalTextureLayerClient implementation.
    virtual unsigned prepareTexture(WebKit::WebTextureUpdater&) OVERRIDE;
    virtual WebKit::WebGraphicsContext3D* context() OVERRIDE;

    // SkDeferredCanvas::NotificationClient implementation
    virtual void prepareForDraw() OVERRIDE;
    virtual void storageAllocatedForRecordingChanged(size_t) OVERRIDE;
    virtual void flushedDrawCommands() OVERRIDE;
    virtual void skippedPendingDrawCommands() OVERRIDE;

    // Methods used by Canvas2DLayerManager
    virtual size_t freeMemoryIfPossible(size_t); // virtual for mocking
    virtual void flush(); // virtual for mocking
    virtual size_t storageAllocatedForRecording(); // virtual for faking
    size_t bytesAllocated() const {return m_bytesAllocated;}
    void limitPendingFrames();

    SkCanvas* skCanvas(SkDevice*);
    WebKit::WebLayer* layer();
    void contextAcquired();

    unsigned backBufferTexture();

protected:
    Canvas2DLayerBridge(PassRefPtr<GraphicsContext3D>, const IntSize&, ThreadMode, unsigned textureId);

    unsigned m_backBufferTexture;
    IntSize m_size;
    SkDeferredCanvas* m_canvas;
    OwnPtr<WebKit::WebExternalTextureLayer> m_layer;
    RefPtr<GraphicsContext3D> m_context;
    size_t m_bytesAllocated;
    bool m_didRecordDrawCommand;
    int m_framesPending;

    friend class WTF::DoublyLinkedListNode<Canvas2DLayerBridge>;
    Canvas2DLayerBridge* m_next;
    Canvas2DLayerBridge* m_prev;
};

}

#endif
