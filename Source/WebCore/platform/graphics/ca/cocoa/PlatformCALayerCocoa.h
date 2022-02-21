/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#include "PlatformCALayer.h"

OBJC_CLASS NSObject;

namespace WebCore {

class PlatformCALayerCocoa final : public PlatformCALayer {
public:
    static Ref<PlatformCALayerCocoa> create(LayerType, PlatformCALayerClient*);
    
    // This function passes the layer as a void* rather than a PlatformLayer because PlatformLayer
    // is defined differently for Obj C and C++. This allows callers from both languages.
    static Ref<PlatformCALayerCocoa> create(void* platformLayer, PlatformCALayerClient*);

    WEBCORE_EXPORT static LayerType layerTypeForPlatformLayer(PlatformLayer*);

    ~PlatformCALayerCocoa();

    void setOwner(PlatformCALayerClient*) override;

    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const FloatRect& dirtyRect) override;

    void copyContentsFromLayer(PlatformCALayer*) override;

    PlatformCALayer* superlayer() const override;
    void removeFromSuperlayer() override;
    void setSublayers(const PlatformCALayerList&) override;
    PlatformCALayerList sublayersForLogging() const override;
    void removeAllSublayers() override;
    void appendSublayer(PlatformCALayer&) override;
    void insertSublayer(PlatformCALayer&, size_t index) override;
    void replaceSublayer(PlatformCALayer& reference, PlatformCALayer&) override;
    const PlatformCALayerList* customSublayers() const override { return m_customSublayers.get(); }
    void adoptSublayers(PlatformCALayer& source) override;

    void addAnimationForKey(const String& key, PlatformCAAnimation&) override;
    void removeAnimationForKey(const String& key) override;
    RefPtr<PlatformCAAnimation> animationForKey(const String& key) override;
    void animationStarted(const String& key, MonotonicTime beginTime) override;
    void animationEnded(const String& key) override;

    void setMask(PlatformCALayer*) override;

    bool isOpaque() const override;
    void setOpaque(bool) override;

    FloatRect bounds() const override;
    void setBounds(const FloatRect&) override;

    FloatPoint3D position() const override;
    void setPosition(const FloatPoint3D&) override;

    FloatPoint3D anchorPoint() const override;
    void setAnchorPoint(const FloatPoint3D&) override;

    TransformationMatrix transform() const override;
    void setTransform(const TransformationMatrix&) override;

    TransformationMatrix sublayerTransform() const override;
    void setSublayerTransform(const TransformationMatrix&) override;

    bool isHidden() const override;
    void setHidden(bool) override;

    bool contentsHidden() const override;
    void setContentsHidden(bool) override;

    bool userInteractionEnabled() const override;
    void setUserInteractionEnabled(bool) override;

    void setBackingStoreAttached(bool) override;
    bool backingStoreAttached() const override;

    bool geometryFlipped() const override;
    WEBCORE_EXPORT void setGeometryFlipped(bool) override;

    bool isDoubleSided() const override;
    void setDoubleSided(bool) override;

    bool masksToBounds() const override;
    void setMasksToBounds(bool) override;

    bool acceleratesDrawing() const override;
    void setAcceleratesDrawing(bool) override;

    bool wantsDeepColorBackingStore() const override;
    void setWantsDeepColorBackingStore(bool) override;

    bool supportsSubpixelAntialiasedText() const override;
    void setSupportsSubpixelAntialiasedText(bool) override;

    bool hasContents() const override;
    CFTypeRef contents() const override;
    void setContents(CFTypeRef) override;
    void clearContents() override;
#if HAVE(IOSURFACE)
    void setContents(const WebCore::IOSurface&) override;
    void setContents(const WTF::MachSendRight&) override;
#endif

    void setContentsRect(const FloatRect&) override;

    void setMinificationFilter(FilterType) override;
    void setMagnificationFilter(FilterType) override;

    Color backgroundColor() const override;
    void setBackgroundColor(const Color&) override;

    void setBorderWidth(float) override;

    void setBorderColor(const Color&) override;

    float opacity() const override;
    void setOpacity(float) override;
    void setFilters(const FilterOperations&) override;
    WEBCORE_EXPORT static bool filtersCanBeComposited(const FilterOperations&);
    void copyFiltersFrom(const PlatformCALayer&) override;

#if ENABLE(CSS_COMPOSITING)
    void setBlendMode(BlendMode) override;
#endif

    void setName(const String&) override;

    void setSpeed(float) override;

    void setTimeOffset(CFTimeInterval) override;

    float contentsScale() const override;
    void setContentsScale(float) override;

    float cornerRadius() const override;
    void setCornerRadius(float) override;

    void setEdgeAntialiasingMask(unsigned) override;

    FloatRoundedRect shapeRoundedRect() const override;
    void setShapeRoundedRect(const FloatRoundedRect&) override;

    Path shapePath() const override;
    void setShapePath(const Path&) override;

    WindRule shapeWindRule() const override;
    void setShapeWindRule(WindRule) override;

    GraphicsLayer::CustomAppearance customAppearance() const override { return m_customAppearance; }
    void updateCustomAppearance(GraphicsLayer::CustomAppearance) override;

    const EventRegion* eventRegion() const override { return &m_eventRegion; }
    void setEventRegion(const EventRegion&) override;

#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeID scrollingNodeID() const override { return m_scrollingNodeID; }
    void setScrollingNodeID(ScrollingNodeID nodeID) override { m_scrollingNodeID = nodeID; }
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    bool isSeparated() const override;
    void setIsSeparated(bool) override;

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    bool isSeparatedPortal() const override;
    void setIsSeparatedPortal(bool) override;

    bool isDescendentOfSeparatedPortal() const override;
    void setIsDescendentOfSeparatedPortal(bool) override;
#endif
#endif

    TiledBacking* tiledBacking() override;

    Ref<PlatformCALayer> clone(PlatformCALayerClient* owner) const override;

    Ref<PlatformCALayer> createCompatibleLayer(PlatformCALayer::LayerType, PlatformCALayerClient*) const override;

    void enumerateRectsBeingDrawn(GraphicsContext&, void (^block)(FloatRect)) override;

    unsigned backingStoreBytesPerPixel() const override;

private:
    PlatformCALayerCocoa(LayerType, PlatformCALayerClient* owner);
    PlatformCALayerCocoa(PlatformLayer*, PlatformCALayerClient* owner);

    void commonInit();

    bool isPlatformCALayerCocoa() const override { return true; }

    bool requiresCustomAppearanceUpdateOnBoundsChange() const;

    void updateContentsFormat();

    AVPlayerLayer *avPlayerLayer() const;

    RetainPtr<NSObject> m_delegate;
    std::unique_ptr<PlatformCALayerList> m_customSublayers;
    GraphicsLayer::CustomAppearance m_customAppearance { GraphicsLayer::CustomAppearance::None };
    std::unique_ptr<FloatRoundedRect> m_shapeRoundedRect;
#if ENABLE(SCROLLING_THREAD)
    ScrollingNodeID m_scrollingNodeID { 0 };
#endif
    EventRegion m_eventRegion;
    bool m_wantsDeepColorBackingStore { false };
    bool m_supportsSubpixelAntialiasedText { false };
    bool m_backingStoreAttached { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_PLATFORM_CALAYER(WebCore::PlatformCALayerCocoa, isPlatformCALayerCocoa())
