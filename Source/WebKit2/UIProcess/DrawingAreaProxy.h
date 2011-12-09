/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#ifndef DrawingAreaProxy_h
#define DrawingAreaProxy_h

#include "DrawingAreaInfo.h"
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <stdint.h>
#include <wtf/Noncopyable.h>

#if PLATFORM(QT)
class QPainter;
#elif PLATFORM(GTK)
typedef struct _cairo cairo_t;
#endif

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class FloatPoint;
    class TransformationMatrix;
}

namespace WebKit {

class LayerTreeContext;
class LayerTreeHostProxy;
class UpdateInfo;
class WebLayerTreeInfo;
class WebLayerUpdateInfo;
class WebPageProxy;

class DrawingAreaProxy {
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxy);

public:
    virtual ~DrawingAreaProxy();

    DrawingAreaType type() const { return m_type; }

    void didReceiveDrawingAreaProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    virtual void deviceScaleFactorDidChange() = 0;

    // FIXME: These should be pure virtual.
    virtual void visibilityDidChange() { }
    virtual void setBackingStoreIsDiscardable(bool) { }

    virtual void waitForBackingStoreUpdateOnNextPaint() { }

    const WebCore::IntSize& size() const { return m_size; }
    void setSize(const WebCore::IntSize&, const WebCore::IntSize& scrollOffset);

    virtual void pageCustomRepresentationChanged() { }

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
    virtual void updateViewport();
    virtual WebCore::IntRect viewportVisibleRect() const { return contentsRect(); }
    virtual WebCore::IntRect contentsRect() const;
    virtual bool isBackingStoreReady() const { return true; }
    virtual void paintToCurrentGLContext(const WebCore::TransformationMatrix&, float opacity) { }

#if USE(TILED_BACKING_STORE)
    virtual void setVisibleContentsRectAndScale(const WebCore::IntRect& visibleContentsRect, float scale) { }
    virtual void setVisibleContentRectTrajectoryVector(const WebCore::FloatPoint&) { }
    virtual void createTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&) { }
    virtual void updateTileForLayer(int layerID, int tileID, const WebKit::UpdateInfo&) { }
    virtual void removeTileForLayer(int layerID, int tileID) { }
    virtual void didReceiveLayerTreeHostProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    WebPageProxy* page() { return m_webPageProxy; }
#endif
#endif
protected:
    explicit DrawingAreaProxy(DrawingAreaType, WebPageProxy*);

    DrawingAreaType m_type;
    WebPageProxy* m_webPageProxy;

    WebCore::IntSize m_size;
    WebCore::IntSize m_scrollOffset;

#if USE(ACCELERATED_COMPOSITING) && USE(TEXTURE_MAPPER)
    OwnPtr<LayerTreeHostProxy> m_layerTreeHostProxy;
#endif

private:
    virtual void sizeDidChange() = 0;

    // CoreIPC message handlers.
    // FIXME: These should be pure virtual.
    virtual void update(uint64_t backingStoreStateID, const UpdateInfo&) { }
    virtual void didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo&, const LayerTreeContext&) { }
#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) { }
    virtual void exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&) { }
#endif
#if PLATFORM(MAC)
    virtual void didUpdateGeometry() { }
#endif
#if USE(TILED_BACKING_STORE)
    virtual void snapshotTaken(const UpdateInfo&) { }
    virtual void createTile(int tileID, const UpdateInfo& updateInfo) { }
    virtual void updateTile(int tileID, const UpdateInfo& updateInfo) { }
    virtual void didRenderFrame() { }
    virtual void removeTile(int tileID) { }
    virtual void allTileUpdatesProcessed() { }
#endif
};

} // namespace WebKit

#endif // DrawingAreaProxy_h
