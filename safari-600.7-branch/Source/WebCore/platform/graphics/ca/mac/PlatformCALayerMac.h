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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformCALayerMac_h
#define PlatformCALayerMac_h

#include "PlatformCALayer.h"

OBJC_CLASS NSObject;

namespace WebCore {

class PlatformCALayerMac final : public PlatformCALayer {
public:
    static PassRefPtr<PlatformCALayer> create(LayerType, PlatformCALayerClient*);
    
    // This function passes the layer as a void* rather than a PlatformLayer because PlatformLayer
    // is defined differently for Obj C and C++. This allows callers from both languages.
    static PassRefPtr<PlatformCALayer> create(void* platformLayer, PlatformCALayerClient*);

    static LayerType layerTypeForPlatformLayer(PlatformLayer*);

    ~PlatformCALayerMac();

    virtual void setOwner(PlatformCALayerClient*) override;

    virtual void setNeedsDisplay(const FloatRect* dirtyRect = 0) override;

    virtual void copyContentsFromLayer(PlatformCALayer*) override;

    virtual PlatformCALayer* superlayer() const override;
    virtual void removeFromSuperlayer() override;
    virtual void setSublayers(const PlatformCALayerList&) override;
    virtual void removeAllSublayers() override;
    virtual void appendSublayer(PlatformCALayer*) override;
    virtual void insertSublayer(PlatformCALayer*, size_t index) override;
    virtual void replaceSublayer(PlatformCALayer* reference, PlatformCALayer*) override;
    virtual const PlatformCALayerList* customSublayers() const override { return m_customSublayers.get(); }
    virtual void adoptSublayers(PlatformCALayer* source) override;

    virtual void addAnimationForKey(const String& key, PlatformCAAnimation*) override;
    virtual void removeAnimationForKey(const String& key) override;
    virtual PassRefPtr<PlatformCAAnimation> animationForKey(const String& key) override;
    virtual void animationStarted(const String& key, CFTimeInterval beginTime) override;

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
    static bool filtersCanBeComposited(const FilterOperations&);
    virtual void copyFiltersFrom(const PlatformCALayer*) override;
#endif

#if ENABLE(CSS_COMPOSITING)
    virtual void setBlendMode(BlendMode) override;
#endif

    virtual void setName(const String&) override;

    virtual void setSpeed(float) override;

    virtual void setTimeOffset(CFTimeInterval) override;

    virtual float contentsScale() const override;
    virtual void setContentsScale(float) override;

    virtual void setEdgeAntialiasingMask(unsigned) override;

    virtual GraphicsLayer::CustomAppearance customAppearance() const override { return m_customAppearance; }
    virtual void updateCustomAppearance(GraphicsLayer::CustomAppearance) override;

    virtual GraphicsLayer::CustomBehavior customBehavior() const override { return m_customBehavior; }
    virtual void updateCustomBehavior(GraphicsLayer::CustomBehavior) override;

    virtual TiledBacking* tiledBacking() override;

    virtual PassRefPtr<PlatformCALayer> clone(PlatformCALayerClient* owner) const override;

    virtual PassRefPtr<PlatformCALayer> createCompatibleLayer(PlatformCALayer::LayerType, PlatformCALayerClient*) const override;

    virtual void enumerateRectsBeingDrawn(CGContextRef, void (^block)(CGRect)) override;

private:
    PlatformCALayerMac(LayerType, PlatformCALayerClient* owner);
    PlatformCALayerMac(PlatformLayer*, PlatformCALayerClient* owner);

    void commonInit();

    virtual bool isPlatformCALayerMac() const override { return true; }

    bool requiresCustomAppearanceUpdateOnBoundsChange() const;

    RetainPtr<NSObject> m_delegate;
    std::unique_ptr<PlatformCALayerList> m_customSublayers;
    GraphicsLayer::CustomAppearance m_customAppearance;
    GraphicsLayer::CustomBehavior m_customBehavior;
};

PLATFORM_CALAYER_TYPE_CASTS(PlatformCALayerMac, isPlatformCALayerMac())

} // namespace WebCore

#endif // PlatformCALayerMac_h
