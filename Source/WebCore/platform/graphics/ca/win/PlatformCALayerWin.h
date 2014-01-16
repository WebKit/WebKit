/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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

#ifndef PlatformCALayerWin_h
#define PlatformCALayerWin_h

#if USE(ACCELERATED_COMPOSITING)

#include "PlatformCALayer.h"

namespace WebCore {

class PlatformCALayerWin final : public PlatformCALayer {
public:
    static PassRefPtr<PlatformCALayer> create(LayerType, PlatformCALayerClient*);
    static PassRefPtr<PlatformCALayer> create(PlatformLayer*, PlatformCALayerClient*);
    
    ~PlatformCALayerWin();

    virtual bool usesTiledBackingLayer() const override { return false; }

    virtual void setNeedsDisplay(const FloatRect* dirtyRect = 0) override;

    virtual void setContentsChanged() override;

    virtual PlatformCALayer* superlayer() const override;
    virtual void removeFromSuperlayer() override;
    virtual void setSublayers(const PlatformCALayerList&) override;
    virtual void removeAllSublayers() override;
    virtual void appendSublayer(PlatformCALayer*) override;
    virtual void insertSublayer(PlatformCALayer*, size_t index) override;
    virtual void replaceSublayer(PlatformCALayer* reference, PlatformCALayer*) override;
    virtual const PlatformCALayerList* customSublayers() const override { return nullptr; }
    virtual void adoptSublayers(PlatformCALayer* source) override;

    virtual void addAnimationForKey(const String& key, PlatformCAAnimation*) override;
    virtual void removeAnimationForKey(const String& key) override;
    virtual PassRefPtr<PlatformCAAnimation> animationForKey(const String& key) override;
    virtual void animationStarted(CFTimeInterval beginTime) override;

    virtual void setMask(PlatformCALayer*) override;

    virtual bool isOpaque() const override;
    virtual void setOpaque(bool) override;

    virtual FloatRect bounds() const override;
    virtual void setBounds(const FloatRect&) override;

    virtual FloatPoint3D position() const override;
    virtual void setPosition(const FloatPoint3D&) override;

    virtual FloatPoint3D anchorPoint() const override;
    virtual void setAnchorPoint(const FloatPoint3D&) override;

    virtual TransformationMatrix transform() const override;
    virtual void setTransform(const TransformationMatrix&) override;

    virtual TransformationMatrix sublayerTransform() const override;
    virtual void setSublayerTransform(const TransformationMatrix&) override;

    virtual void setHidden(bool) override;

    virtual void setGeometryFlipped(bool) override;

    virtual bool isDoubleSided() const override;
    virtual void setDoubleSided(bool) override;

    virtual bool masksToBounds() const override;
    virtual void setMasksToBounds(bool) override;

    virtual bool acceleratesDrawing() const override;
    virtual void setAcceleratesDrawing(bool) override;

    virtual CFTypeRef contents() const override;
    virtual void setContents(CFTypeRef) override;

    virtual void setContentsRect(const FloatRect&) override;

    virtual void setMinificationFilter(FilterType) override;
    virtual void setMagnificationFilter(FilterType) override;

    virtual Color backgroundColor() const override;
    virtual void setBackgroundColor(const Color&) override;

    virtual void setBorderWidth(float) override;

    virtual void setBorderColor(const Color&) override;

    virtual float opacity() const override;
    virtual void setOpacity(float) override;

#if ENABLE(CSS_FILTERS)
    virtual void setFilters(const FilterOperations&) override;
    static bool filtersCanBeComposited(const FilterOperations&) { return false; }
    virtual void copyFiltersFrom(const PlatformCALayer*) override;
#endif

    virtual void setName(const String&) override;

    virtual void setSpeed(float) override;

    virtual void setTimeOffset(CFTimeInterval) override;

    virtual float contentsScale() const override;
    virtual void setContentsScale(float) override;

    virtual void setEdgeAntialiasingMask(unsigned) override { ASSERT_NOT_REACHED(); }

    virtual GraphicsLayer::CustomAppearance customAppearance() const override { return m_customAppearance; }
    virtual void updateCustomAppearance(GraphicsLayer::CustomAppearance customAppearance) override { m_customAppearance = customAppearance; }

    virtual TiledBacking* tiledBacking() override { return nullptr; }
    
    virtual PlatformCALayer* rootLayer() const override;
    virtual void setNeedsLayout() override;
    virtual void setNeedsCommit() override;

#ifndef NDEBUG
    virtual void printTree() const override;
#endif

    virtual PassRefPtr<PlatformCALayer> clone(PlatformCALayerClient* owner) const override;

    virtual PassRefPtr<PlatformCALayer> createCompatibleLayer(PlatformCALayer::LayerType, PlatformCALayerClient*) const override;

private:
    PlatformCALayerWin(LayerType, PlatformLayer*, PlatformCALayerClient* owner);

    HashMap<String, RefPtr<PlatformCAAnimation>> m_animations;
    GraphicsLayer::CustomAppearance m_customAppearance;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PlatformCALayerWin_h
