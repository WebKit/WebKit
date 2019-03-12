/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#include "FloatRoundedRect.h"
#include "GraphicsLayer.h"
#include <QuartzCore/CABase.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>

OBJC_CLASS AVPlayerLayer;

#if PLATFORM(COCOA)
typedef struct CGContext *CGContextRef;
#endif

namespace WebCore {

class LayerPool;
class PlatformCALayer;
class PlatformCAAnimation;
class PlatformCALayerClient;

typedef Vector<RefPtr<PlatformCALayer>> PlatformCALayerList;

class WEBCORE_EXPORT PlatformCALayer : public RefCounted<PlatformCALayer> {
#if PLATFORM(COCOA)
    friend class PlatformCALayerCocoa;
#elif PLATFORM(WIN)
    friend class PlatformCALayerWin;
#endif
public:
    static CFTimeInterval currentTimeToMediaTime(MonotonicTime t) { return CACurrentMediaTime() + (t - MonotonicTime::now()).seconds(); }

    // LayerTypeRootLayer is used on some platforms. It has no backing store, so setNeedsDisplay
    // should not call CACFLayerSetNeedsDisplay, but rather just notify the renderer that it
    // has changed and should be re-rendered.
    enum LayerType {
        LayerTypeLayer,
        LayerTypeWebLayer,
        LayerTypeSimpleLayer,
        LayerTypeTransformLayer,
        LayerTypeTiledBackingLayer,
        LayerTypePageTiledBackingLayer,
        LayerTypeTiledBackingTileLayer,
        LayerTypeRootLayer,
        LayerTypeAVPlayerLayer,
        LayerTypeContentsProvidedLayer,
        LayerTypeBackdropLayer,
        LayerTypeShapeLayer,
        LayerTypeLightSystemBackdropLayer,
        LayerTypeDarkSystemBackdropLayer,
        LayerTypeScrollContainerLayer,
        LayerTypeEditableImageLayer,
        LayerTypeCustom,
    };
    enum FilterType { Linear, Nearest, Trilinear };

    virtual Ref<PlatformCALayer> clone(PlatformCALayerClient*) const = 0;

    virtual ~PlatformCALayer();

    GraphicsLayer::PlatformLayerID layerID() const { return m_layerID; }

    virtual bool isPlatformCALayerCocoa() const { return false; }
    virtual bool isPlatformCALayerRemote() const { return false; }
    virtual bool isPlatformCALayerRemoteCustom() const { return false; }

    // This function passes the layer as a void* rather than a PlatformLayer because PlatformLayer
    // is defined differently for Obj C and C++. This allows callers from both languages.
    static PlatformCALayer* platformCALayer(void* platformLayer);

    virtual PlatformLayer* platformLayer() const { return m_layer.get(); }

    bool usesTiledBackingLayer() const { return layerType() == LayerTypePageTiledBackingLayer || layerType() == LayerTypeTiledBackingLayer; }

    PlatformCALayerClient* owner() const { return m_owner; }
    virtual void setOwner(PlatformCALayerClient* owner) { m_owner = owner; }

    virtual void animationStarted(const String& key, MonotonicTime beginTime) = 0;
    virtual void animationEnded(const String& key) = 0;

    virtual void setNeedsDisplay() = 0;
    virtual void setNeedsDisplayInRect(const FloatRect& dirtyRect) = 0;

    virtual void copyContentsFromLayer(PlatformCALayer*) = 0;

    LayerType layerType() const { return m_layerType; }
    
    bool canHaveBackingStore() const;

    virtual PlatformCALayer* superlayer() const = 0;
    virtual void removeFromSuperlayer() = 0;
    virtual void setSublayers(const PlatformCALayerList&) = 0;
    virtual void removeAllSublayers() = 0;
    virtual void appendSublayer(PlatformCALayer&) = 0;
    virtual void insertSublayer(PlatformCALayer&, size_t index) = 0;
    virtual void replaceSublayer(PlatformCALayer& reference, PlatformCALayer&) = 0;

    // A list of sublayers that GraphicsLayerCA should maintain as the first sublayers.
    virtual const PlatformCALayerList* customSublayers() const = 0;

    // This method removes the sublayers from the source and reparents them to the current layer.
    // Any sublayers previously in the current layer are removed.
    virtual void adoptSublayers(PlatformCALayer& source) = 0;

    virtual void addAnimationForKey(const String& key, PlatformCAAnimation&) = 0;
    virtual void removeAnimationForKey(const String& key) = 0;
    virtual RefPtr<PlatformCAAnimation> animationForKey(const String& key) = 0;

    virtual void setMask(PlatformCALayer*) = 0;

    virtual bool isOpaque() const = 0;
    virtual void setOpaque(bool) = 0;

    virtual FloatRect bounds() const = 0;
    virtual void setBounds(const FloatRect&) = 0;

    virtual FloatPoint3D position() const = 0;
    virtual void setPosition(const FloatPoint3D&) = 0;
    void setPosition(const FloatPoint& pos) { setPosition(FloatPoint3D(pos.x(), pos.y(), 0)); }

    virtual FloatPoint3D anchorPoint() const = 0;
    virtual void setAnchorPoint(const FloatPoint3D&) = 0;

    virtual TransformationMatrix transform() const = 0;
    virtual void setTransform(const TransformationMatrix&) = 0;

    virtual TransformationMatrix sublayerTransform() const = 0;
    virtual void setSublayerTransform(const TransformationMatrix&) = 0;

    virtual bool isHidden() const = 0;
    virtual void setHidden(bool) = 0;

    // Used to disable user interaction for some platforms.
    virtual bool contentsHidden() const = 0;
    virtual void setContentsHidden(bool) = 0;
    virtual bool userInteractionEnabled() const = 0;
    virtual void setUserInteractionEnabled(bool) = 0;

    virtual bool geometryFlipped() const = 0;
    virtual void setGeometryFlipped(bool) = 0;

    virtual bool isDoubleSided() const = 0;
    virtual void setDoubleSided(bool) = 0;

    virtual bool masksToBounds() const = 0;
    virtual void setMasksToBounds(bool) = 0;

    virtual bool acceleratesDrawing() const = 0;
    virtual void setAcceleratesDrawing(bool) = 0;

    virtual bool wantsDeepColorBackingStore() const = 0;
    virtual void setWantsDeepColorBackingStore(bool) = 0;

    virtual bool supportsSubpixelAntialiasedText() const = 0;
    virtual void setSupportsSubpixelAntialiasedText(bool) = 0;

    virtual bool hasContents() const = 0;
    virtual CFTypeRef contents() const = 0;
    virtual void setContents(CFTypeRef) = 0;

    virtual void setContentsRect(const FloatRect&) = 0;

    virtual void setBackingStoreAttached(bool) = 0;
    virtual bool backingStoreAttached() const = 0;

    virtual void setMinificationFilter(FilterType) = 0;
    virtual void setMagnificationFilter(FilterType) = 0;

    virtual Color backgroundColor() const = 0;
    virtual void setBackgroundColor(const Color&) = 0;

    virtual void setBorderWidth(float) = 0;

    virtual void setBorderColor(const Color&) = 0;

    virtual float opacity() const = 0;
    virtual void setOpacity(float) = 0;
    virtual void setFilters(const FilterOperations&) = 0;
    virtual void copyFiltersFrom(const PlatformCALayer&) = 0;

#if ENABLE(CSS_COMPOSITING)
    virtual void setBlendMode(BlendMode) = 0;
#endif

    virtual void setName(const String&) = 0;

    virtual void setSpeed(float) = 0;

    virtual void setTimeOffset(CFTimeInterval) = 0;

    virtual float contentsScale() const = 0;
    virtual void setContentsScale(float) = 0;

    virtual float cornerRadius() const = 0;
    virtual void setCornerRadius(float) = 0;

    virtual void setEdgeAntialiasingMask(unsigned) = 0;
    
    // Only used by LayerTypeShapeLayer.
    virtual FloatRoundedRect shapeRoundedRect() const = 0;
    virtual void setShapeRoundedRect(const FloatRoundedRect&) = 0;

    // Only used by LayerTypeShapeLayer.
    virtual Path shapePath() const = 0;
    virtual void setShapePath(const Path&) = 0;

    virtual WindRule shapeWindRule() const = 0;
    virtual void setShapeWindRule(WindRule) = 0;

    virtual const Region* eventRegion() const = 0;
    virtual void setEventRegion(const Region*) = 0;
    
    virtual GraphicsLayer::CustomAppearance customAppearance() const = 0;
    virtual void updateCustomAppearance(GraphicsLayer::CustomAppearance) = 0;

    virtual GraphicsLayer::EmbeddedViewID embeddedViewID() const = 0;

    virtual TiledBacking* tiledBacking() = 0;

    virtual void drawTextAtPoint(CGContextRef, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, const char* text, size_t length, CGFloat strokeWidthAsPercentageOfFontSize = 0, Color strokeColor = Color()) const;

    static void flipContext(CGContextRef, CGFloat height);
    
    virtual unsigned backingStoreBytesPerPixel() const { return 4; }

#if PLATFORM(WIN)
    virtual PlatformCALayer* rootLayer() const = 0;
    virtual void setNeedsLayout() = 0;
    virtual void setNeedsCommit() = 0;
    virtual String layerTreeAsString() const = 0;
#endif // PLATFORM(WIN)

#if PLATFORM(IOS_FAMILY)
    bool isWebLayer();
    void setBoundsOnMainThread(CGRect);
    void setPositionOnMainThread(CGPoint);
    void setAnchorPointOnMainThread(FloatPoint3D);
#endif

    virtual Ref<PlatformCALayer> createCompatibleLayer(LayerType, PlatformCALayerClient*) const = 0;
    Ref<PlatformCALayer> createCompatibleLayerOrTakeFromPool(LayerType, PlatformCALayerClient*, IntSize);

#if PLATFORM(COCOA)
    virtual void enumerateRectsBeingDrawn(CGContextRef, void (^block)(CGRect)) = 0;
#endif

    static const unsigned webLayerMaxRectsToPaint = 5;
#if COMPILER(MSVC)
    static const float webLayerWastedSpaceThreshold;
#else
    constexpr static const float webLayerWastedSpaceThreshold = 0.75f;
#endif

    typedef Vector<FloatRect, webLayerMaxRectsToPaint> RepaintRectList;
        
    // Functions allows us to share implementation across WebTiledLayer and WebLayer
    static RepaintRectList collectRectsToPaint(CGContextRef, PlatformCALayer*);
    static void drawLayerContents(CGContextRef, PlatformCALayer*, RepaintRectList& dirtyRects, GraphicsLayerPaintBehavior);
    static void drawRepaintIndicator(CGContextRef, PlatformCALayer*, int repaintCount, CGColorRef customBackgroundColor);
    static CGRect frameForLayer(const PlatformLayer*);

    void moveToLayerPool();

protected:
    PlatformCALayer(LayerType, PlatformCALayerClient* owner);

    virtual LayerPool& layerPool();

    const LayerType m_layerType;
    const GraphicsLayer::PlatformLayerID m_layerID;
    RetainPtr<PlatformLayer> m_layer;
    PlatformCALayerClient* m_owner;
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformCALayer::LayerType);
WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, PlatformCALayer::FilterType);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_PLATFORM_CALAYER(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(ToValueTypeName) \
    static bool isType(const WebCore::PlatformCALayer& layer) { return layer.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
