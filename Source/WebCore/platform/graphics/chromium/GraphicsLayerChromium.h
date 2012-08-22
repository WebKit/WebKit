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
#include <public/WebLayer.h>
#include <wtf/HashMap.h>

namespace WebCore {

class LayerChromium;
class LinkHighlight;
class Path;

class GraphicsLayerChromium : public GraphicsLayer, public GraphicsContextPainter, public WebKit::WebAnimationDelegate {
public:
    GraphicsLayerChromium(GraphicsLayerClient*);
    virtual ~GraphicsLayerChromium();

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
    virtual void clearBackgroundColor();

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

    virtual void setContentsToImage(Image*);
    virtual void setContentsToMedia(PlatformLayer*);
    virtual void setContentsToCanvas(PlatformLayer*);
    virtual bool hasContentsLayer() const { return !m_contentsLayer.isNull(); }

    virtual bool addAnimation(const KeyframeValueList&, const IntSize& boxSize, const Animation*, const String&, double timeOffset);
    virtual void pauseAnimation(const String& animationName, double timeOffset);
    virtual void removeAnimation(const String& animationName);
    virtual void suspendAnimations(double wallClockTime);
    virtual void resumeAnimations();

    virtual void addLinkHighlight(const Path&);
    virtual void didFinishLinkHighlight();

    virtual PlatformLayer* platformLayer() const;

    virtual void setDebugBackgroundColor(const Color&);
    virtual void setDebugBorder(const Color&, float borderWidth);
    virtual void deviceOrPageScaleFactorChanged();

    // GraphicsContextPainter implementation.
    virtual void paint(GraphicsContext&, const IntRect& clip) OVERRIDE;

    // WebAnimationDelegate implementation.
    virtual void notifyAnimationStarted(double startTime) OVERRIDE;
    virtual void notifyAnimationFinished(double finishTime) OVERRIDE;

    // Exposed for tests.
    WebKit::WebLayer contentsLayer() const { return m_contentsLayer; }
    float contentsScale() const;

private:
    virtual void willBeDestroyed();

    WebKit::WebLayer primaryLayer() const;

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
    void updateContentsScale();

    void setupContentsLayer(WebKit::WebLayer);

    int mapAnimationNameToId(const String& animationName);

    String m_nameBase;

    WebKit::WebContentLayer m_layer;
    WebKit::WebLayer m_transformLayer;
    WebKit::WebLayer m_contentsLayer;

    OwnPtr<OpaqueRectTrackingContentLayerDelegate> m_opaqueRectTrackingContentLayerDelegate;

    enum ContentsLayerPurpose {
        NoContentsLayer = 0,
        ContentsLayerForImage,
        ContentsLayerForVideo,
        ContentsLayerForCanvas,
    };

    ContentsLayerPurpose m_contentsLayerPurpose;
    bool m_contentsLayerHasBackgroundColor : 1;
    bool m_inSetChildren;
    bool m_pageScaleChanged;

    RefPtr<LinkHighlight> m_linkHighlight;

    typedef HashMap<String, int> AnimationIdMap;
    AnimationIdMap m_animationIdMap;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
