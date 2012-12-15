/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef GraphicsLayerChromium_h
#define GraphicsLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "OpaqueRectTrackingContentLayerDelegate.h"

#include <public/WebAnimationDelegate.h>
#include <public/WebContentLayer.h>
#include <public/WebImageLayer.h>
#include <public/WebLayer.h>
#include <public/WebLayerScrollClient.h>
#include <wtf/HashMap.h>

namespace WebCore {

class Path;
class ScrollableArea;

class LinkHighlightClient {
public:
    virtual void invalidate() = 0;
    virtual void clearCurrentGraphicsLayer() = 0;
    virtual WebKit::WebLayer* layer() = 0;

protected:
    virtual ~LinkHighlightClient() { }
};

class GraphicsLayerChromium : public GraphicsLayer, public GraphicsContextPainter, public WebKit::WebAnimationDelegate, public WebKit::WebLayerScrollClient {
public:
    GraphicsLayerChromium(GraphicsLayerClient*);
    virtual ~GraphicsLayerChromium();

    virtual void willBeDestroyed() OVERRIDE;

    virtual void setName(const String&);

    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);

    virtual void removeFromParent();

    virtual void setPosition(const FloatPoint&);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);

    virtual void setChildrenTransform(const TransformationMatrix&);

    virtual void setPreserves3D(bool);
    virtual void setMasksToBounds(bool);
    virtual void setDrawsContent(bool);
    virtual void setContentsVisible(bool);
    virtual void setMaskLayer(GraphicsLayer*);

    virtual void setBackgroundColor(const Color&);

    virtual void setContentsOpaque(bool);
    virtual void setBackfaceVisibility(bool);

    virtual void setReplicatedByLayer(GraphicsLayer*);

    virtual void setOpacity(float);

    // Returns true if filter can be rendered by the compositor
    virtual bool setFilters(const FilterOperations&);
    void setBackgroundFilters(const FilterOperations&);

    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setContentsNeedsDisplay();

    virtual void setContentsRect(const IntRect&);

    static void registerContentsLayer(WebKit::WebLayer*);
    static void unregisterContentsLayer(WebKit::WebLayer*);

    virtual void setContentsToImage(Image*);
    virtual void setContentsToMedia(PlatformLayer*);
    virtual void setContentsToCanvas(PlatformLayer*);
    virtual bool hasContentsLayer() const { return m_contentsLayer; }

    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, const String&, double timeOffset);
    virtual void pauseAnimation(const String& animationName, double timeOffset);
    virtual void removeAnimation(const String& animationName);
    virtual void suspendAnimations(double wallClockTime);
    virtual void resumeAnimations();

    void setLinkHighlight(LinkHighlightClient*);
    // Next function for testing purposes.
    LinkHighlightClient* linkHighlight() { return m_linkHighlight; }

    virtual WebKit::WebLayer* platformLayer() const;

    virtual void setAppliesPageScale(bool appliesScale) OVERRIDE;
    virtual bool appliesPageScale() const OVERRIDE;

    void setScrollableArea(ScrollableArea* scrollableArea) { m_scrollableArea = scrollableArea; }
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double startTime) OVERRIDE;
    virtual void notifyAnimationFinished(double finishTime) OVERRIDE;

    // WebLayerScrollClient implementation.
    virtual void didScroll() OVERRIDE;

    WebKit::WebContentLayer* contentLayer() const { return m_layer.get(); }

    // Exposed for tests.
    WebKit::WebLayer* contentsLayer() const { return m_contentsLayer; }

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

private:
    void updateNames();
    void updateChildList();
    void updateLayerPosition();
    void updateLayerSize();
    void updateAnchorPoint();
    void updateTransform();
    void updateChildrenTransform();
    void updateMasksToBounds();
    void updateLayerPreserves3D();
    void updateLayerIsDrawable();
    void updateLayerBackgroundColor();

    void updateContentsImage();
    void updateContentsVideo();
    void updateContentsRect();

    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForVideo,
        ContentsLayerForCanvas,
    };

    void setContentsTo(ContentsLayerPurpose, WebKit::WebLayer*);
    void setupContentsLayer(WebKit::WebLayer*);
    void clearContentsLayerIfUnregistered();
    WebKit::WebLayer* contentsLayerIfRegistered();

    String m_nameBase;

    OwnPtr<WebKit::WebContentLayer> m_layer;
    OwnPtr<WebKit::WebLayer> m_transformLayer;
    OwnPtr<WebKit::WebImageLayer> m_imageLayer;
    WebKit::WebLayer* m_contentsLayer;
    // We don't have ownership of m_contentsLayer, but we do want to know if a given layer is the
    // same as our current layer in setContentsTo(). Since m_contentsLayer may be deleted at this point,
    // we stash an ID away when we know m_contentsLayer is alive and use that for comparisons from that point
    // on.
    int m_contentsLayerId;

    LinkHighlightClient* m_linkHighlight;

    OwnPtr<OpaqueRectTrackingContentLayerDelegate> m_opaqueRectTrackingContentLayerDelegate;

    ContentsLayerPurpose m_contentsLayerPurpose;
    bool m_inSetChildren;

    typedef HashMap<String, int> AnimationIdMap;
    AnimationIdMap m_animationIdMap;

    ScrollableArea* m_scrollableArea;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
