/*
 * Copyright (C) 2010, 2013, 2015 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>

namespace WebCore {

class PlatformCALayerWin final : public PlatformCALayer {
public:
    static Ref<PlatformCALayer> create(LayerType, PlatformCALayerClient*);
    static Ref<PlatformCALayer> create(PlatformLayer*, PlatformCALayerClient*);
    
    ~PlatformCALayerWin();

    void setNeedsDisplayInRect(const FloatRect& dirtyRect) override;
    void setNeedsDisplay() override;

    void copyContentsFromLayer(PlatformCALayer*) override;

    PlatformCALayer* superlayer() const override;
    void removeFromSuperlayer() override;
    void setSublayers(const PlatformCALayerList&) override;
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
    void setGeometryFlipped(bool) override;

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
    static bool filtersCanBeComposited(const FilterOperations&) { return false; }
    void copyFiltersFrom(const PlatformCALayer&) override;

    void setName(const String&) override;

    void setSpeed(float) override;

    void setTimeOffset(CFTimeInterval) override;

    float contentsScale() const override;
    void setContentsScale(float) override;

    float cornerRadius() const override;
    void setCornerRadius(float) override;

    FloatRoundedRect shapeRoundedRect() const override;
    void setShapeRoundedRect(const FloatRoundedRect&) override;

    Path shapePath() const override;
    void setShapePath(const Path&) override;

    WindRule shapeWindRule() const override;
    void setShapeWindRule(WindRule) override;

    void setEdgeAntialiasingMask(unsigned) override;

    GraphicsLayer::CustomAppearance customAppearance() const override { return m_customAppearance; }
    void updateCustomAppearance(GraphicsLayer::CustomAppearance customAppearance) override { m_customAppearance = customAppearance; }

    TiledBacking* tiledBacking() override;
    
    PlatformCALayer* rootLayer() const override;
    void setNeedsLayout() override;
    void setNeedsCommit() override;
    void drawTextAtPoint(CGContextRef, CGFloat x, CGFloat y, CGSize scale, CGFloat fontSize, const char* text, size_t length, CGFloat strokeWidth, Color strokeColor) const override;

    String layerTreeAsString() const override;

    Ref<PlatformCALayer> clone(PlatformCALayerClient* owner) const override;

    Ref<PlatformCALayer> createCompatibleLayer(PlatformCALayer::LayerType, PlatformCALayerClient*) const override;

private:
    PlatformCALayerWin(LayerType, PlatformLayer*, PlatformCALayerClient* owner);

    HashMap<String, RefPtr<PlatformCAAnimation>> m_animations;
    std::unique_ptr<PlatformCALayerList> m_customSublayers;
    GraphicsLayer::CustomAppearance m_customAppearance { GraphicsLayer::CustomAppearance::None };
};

}
