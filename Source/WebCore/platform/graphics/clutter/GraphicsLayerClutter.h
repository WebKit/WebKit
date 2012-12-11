/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Collabora Ltd.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef GraphicsLayerClutter_h
#define GraphicsLayerClutter_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "Image.h"
#include "ImageSource.h"
#include "PlatformClutterLayerClient.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

#include <clutter/clutter.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _GraphicsLayerActor GraphicsLayerActor;

namespace WebCore {

class TransformState;

typedef Vector<GRefPtr<GraphicsLayerActor> > GraphicsLayerActorList;

class GraphicsLayerClutter : public GraphicsLayer, public PlatformClutterLayerClient {
public:
    enum LayerType { LayerTypeLayer, LayerTypeWebLayer, LayerTypeVideoLayer, LayerTypeTransformLayer, LayerTypeRootLayer, LayerTypeCustom };

    GraphicsLayerClutter(GraphicsLayerClient*);
    virtual ~GraphicsLayerClutter();

    virtual ClutterActor* platformLayer() const;
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer* sibling);
    virtual void removeFromParent();
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);
    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void setParent(GraphicsLayer*);

    virtual void setDrawsContent(bool);
    virtual void setAnchorPoint(const FloatPoint3D&);
    virtual void setPosition(const FloatPoint&);
    virtual void setSize(const FloatSize&);

    virtual void setTransform(const TransformationMatrix&);
    virtual void setName(const String&);
    virtual void setNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);

    virtual void flushCompositingState(const FloatRect&);
    virtual void flushCompositingStateForThisLayerOnly();

    void recursiveCommitChanges(const TransformState&, float pageScaleFactor = 1, const FloatPoint& positionRelativeToBase = FloatPoint(), bool affectedByPageScale = false);

private:
    FloatPoint computePositionRelativeToBase(float& pageScale) const;
    void commitLayerChangesBeforeSublayers(float pageScaleFactor, const FloatPoint& positionRelativeToBase);
    void commitLayerChangesAfterSublayers();

    virtual void platformClutterLayerAnimationStarted(double beginTime);
    virtual void platformClutterLayerPaintContents(GraphicsContext&, const IntRect& clip);

    GraphicsLayerActor* primaryLayer() const { return m_layer.get(); }
    GraphicsLayerActor* layerForSuperlayer() const;
    enum LayerChange {
        NoChange = 0,
        NameChanged = 1 << 1,
        ChildrenChanged = 1 << 2, // also used for content layer, and preserves-3d, and size if tiling changes?
        GeometryChanged = 1 << 3,
        TransformChanged = 1 << 4,
        ChildrenTransformChanged = 1 << 5,
        Preserves3DChanged = 1 << 6,
        MasksToBoundsChanged = 1 << 7,
        DrawsContentChanged = 1 << 8, // need this?
        BackgroundColorChanged = 1 << 9,
        ContentsOpaqueChanged = 1 << 10,
        BackfaceVisibilityChanged = 1 << 11,
        OpacityChanged = 1 << 12,
        AnimationChanged = 1 << 13,
        DirtyRectsChanged = 1 << 14,
        ContentsImageChanged = 1 << 15,
        ContentsMediaLayerChanged = 1 << 16,
        ContentsCanvasLayerChanged = 1 << 17,
        ContentsRectChanged = 1 << 18,
        MaskLayerChanged = 1 << 19,
        ReplicatedLayerChanged = 1 << 20,
        ContentsNeedsDisplay = 1 << 21,
        AcceleratesDrawingChanged = 1 << 22,
        ContentsScaleChanged = 1 << 23
    };

    typedef unsigned LayerChangeFlags;
    void noteLayerPropertyChanged(LayerChangeFlags);
    void noteSublayersChanged();

    void updateBackfaceVisibility();
    void updateLayerNames();
    void updateSublayerList();
    void updateGeometry(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);
    void updateTransform();
    void updateLayerDrawsContent(float pixelAlignmentScale, const FloatPoint& positionRelativeToBase);

    void repaintLayerDirtyRects();

    GRefPtr<GraphicsLayerActor> m_layer;

    Vector<FloatRect> m_dirtyRects;
    LayerChangeFlags m_uncommittedChanges;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // GraphicsLayerClutter_h
