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

class PlatformCALayerWin FINAL : public PlatformCALayer {
public:
    static PassRefPtr<PlatformCALayer> create(LayerType, PlatformCALayerClient*);
    static PassRefPtr<PlatformCALayer> create(PlatformLayer*, PlatformCALayerClient*);
    
    ~PlatformCALayerWin();

    virtual bool usesTiledBackingLayer() const OVERRIDE { return false; }

    virtual void setNeedsDisplay(const FloatRect* dirtyRect = 0) OVERRIDE;

    virtual void setContentsChanged() OVERRIDE;

    virtual PlatformCALayer* superlayer() const OVERRIDE;
    virtual void removeFromSuperlayer() OVERRIDE;
    virtual void setSublayers(const PlatformCALayerList&) OVERRIDE;
    virtual void removeAllSublayers() OVERRIDE;
    virtual void appendSublayer(PlatformCALayer*) OVERRIDE;
    virtual void insertSublayer(PlatformCALayer*, size_t index) OVERRIDE;
    virtual void replaceSublayer(PlatformCALayer* reference, PlatformCALayer*) OVERRIDE;
    virtual const PlatformCALayerList* customSublayers() const OVERRIDE { return nullptr; }
    virtual void adoptSublayers(PlatformCALayer* source) OVERRIDE;

    virtual void addAnimationForKey(const String& key, PlatformCAAnimation*) OVERRIDE;
    virtual void removeAnimationForKey(const String& key) OVERRIDE;
    virtual PassRefPtr<PlatformCAAnimation> animationForKey(const String& key) OVERRIDE;
    virtual void animationStarted(CFTimeInterval beginTime) OVERRIDE;

    virtual void setMask(PlatformCALayer*) OVERRIDE;

    virtual bool isOpaque() const OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

    virtual FloatRect bounds() const OVERRIDE;
    virtual void setBounds(const FloatRect&) OVERRIDE;

    virtual FloatPoint3D position() const OVERRIDE;
    virtual void setPosition(const FloatPoint3D&) OVERRIDE;

    virtual FloatPoint3D anchorPoint() const OVERRIDE;
    virtual void setAnchorPoint(const FloatPoint3D&) OVERRIDE;

    virtual TransformationMatrix transform() const OVERRIDE;
    virtual void setTransform(const TransformationMatrix&) OVERRIDE;

    virtual TransformationMatrix sublayerTransform() const OVERRIDE;
    virtual void setSublayerTransform(const TransformationMatrix&) OVERRIDE;

    virtual void setHidden(bool) OVERRIDE;

    virtual void setGeometryFlipped(bool) OVERRIDE;

    virtual bool isDoubleSided() const OVERRIDE;
    virtual void setDoubleSided(bool) OVERRIDE;

    virtual bool masksToBounds() const OVERRIDE;
    virtual void setMasksToBounds(bool) OVERRIDE;

    virtual bool acceleratesDrawing() const OVERRIDE;
    virtual void setAcceleratesDrawing(bool) OVERRIDE;

    virtual CFTypeRef contents() const OVERRIDE;
    virtual void setContents(CFTypeRef) OVERRIDE;

    virtual void setContentsRect(const FloatRect&) OVERRIDE;

    virtual void setMinificationFilter(FilterType) OVERRIDE;
    virtual void setMagnificationFilter(FilterType) OVERRIDE;

    virtual Color backgroundColor() const OVERRIDE;
    virtual void setBackgroundColor(const Color&) OVERRIDE;

    virtual void setBorderWidth(float) OVERRIDE;

    virtual void setBorderColor(const Color&) OVERRIDE;

    virtual float opacity() const OVERRIDE;
    virtual void setOpacity(float) OVERRIDE;

#if ENABLE(CSS_FILTERS)
    virtual void setFilters(const FilterOperations&) OVERRIDE;
    static bool filtersCanBeComposited(const FilterOperations&) { return false; }
    virtual void copyFiltersFrom(const PlatformCALayer*) OVERRIDE;
#endif

    virtual void setName(const String&) OVERRIDE;

    virtual void setFrame(const FloatRect&) OVERRIDE;

    virtual void setSpeed(float) OVERRIDE;

    virtual void setTimeOffset(CFTimeInterval) OVERRIDE;

    virtual float contentsScale() const OVERRIDE;
    virtual void setContentsScale(float) OVERRIDE;

    virtual TiledBacking* tiledBacking() OVERRIDE { return nullptr; }
    
    virtual PlatformCALayer* rootLayer() const OVERRIDE;
    virtual void setNeedsLayout() OVERRIDE;
    virtual void setNeedsCommit() OVERRIDE;

#ifndef NDEBUG
    virtual void printTree() const OVERRIDE;
#endif

    virtual PassRefPtr<PlatformCALayer> clone(PlatformCALayerClient* owner) const OVERRIDE;

private:
    PlatformCALayerWin(LayerType, PlatformLayer*, PlatformCALayerClient* owner);

    HashMap<String, RefPtr<PlatformCAAnimation>> m_animations;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PlatformCALayerWin_h
