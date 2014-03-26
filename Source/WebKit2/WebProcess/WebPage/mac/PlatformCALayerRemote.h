/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef PlatformCALayerRemote_h
#define PlatformCALayerRemote_h

#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/TileController.h>

namespace WebKit {

class RemoteLayerTreeContext;

class PlatformCALayerRemote : public WebCore::PlatformCALayer {
public:
    static PassRefPtr<PlatformCALayerRemote> create(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext*);
    static PassRefPtr<PlatformCALayerRemote> create(PlatformLayer *, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext*);
    static PassRefPtr<PlatformCALayerRemote> create(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext*);

    virtual ~PlatformCALayerRemote();

    virtual bool usesTiledBackingLayer() const override { return layerType() == LayerTypePageTiledBackingLayer || layerType() == LayerTypeTiledBackingLayer; }

    virtual PlatformLayer* platformLayer() const override { return nullptr; }

    void recursiveBuildTransaction(RemoteLayerTreeTransaction&);

    virtual void setNeedsDisplay(const WebCore::FloatRect* dirtyRect = 0) override;

    virtual void setContentsChanged() override;

    virtual WebCore::PlatformCALayer* superlayer() const override;
    virtual void removeFromSuperlayer() override;
    virtual void setSublayers(const WebCore::PlatformCALayerList&) override;
    virtual void removeAllSublayers() override;
    virtual void appendSublayer(WebCore::PlatformCALayer*) override;
    virtual void insertSublayer(WebCore::PlatformCALayer*, size_t index) override;
    virtual void replaceSublayer(WebCore::PlatformCALayer* reference, WebCore::PlatformCALayer*) override;
    virtual const WebCore::PlatformCALayerList* customSublayers() const override { return nullptr; }
    virtual void adoptSublayers(WebCore::PlatformCALayer* source) override;

    virtual void addAnimationForKey(const String& key, WebCore::PlatformCAAnimation*) override;
    virtual void removeAnimationForKey(const String& key) override;
    virtual PassRefPtr<WebCore::PlatformCAAnimation> animationForKey(const String& key) override;
    virtual void animationStarted(CFTimeInterval beginTime) override;

    virtual void setMask(WebCore::PlatformCALayer*) override;

    virtual bool isOpaque() const override;
    virtual void setOpaque(bool) override;

    virtual WebCore::FloatRect bounds() const override;
    virtual void setBounds(const WebCore::FloatRect&) override;

    virtual WebCore::FloatPoint3D position() const override;
    virtual void setPosition(const WebCore::FloatPoint3D&) override;

    virtual WebCore::FloatPoint3D anchorPoint() const override;
    virtual void setAnchorPoint(const WebCore::FloatPoint3D&) override;

    virtual WebCore::TransformationMatrix transform() const override;
    virtual void setTransform(const WebCore::TransformationMatrix&) override;

    virtual WebCore::TransformationMatrix sublayerTransform() const override;
    virtual void setSublayerTransform(const WebCore::TransformationMatrix&) override;

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

    virtual void setContentsRect(const WebCore::FloatRect&) override;

    virtual void setMinificationFilter(WebCore::PlatformCALayer::FilterType) override;
    virtual void setMagnificationFilter(WebCore::PlatformCALayer::FilterType) override;

    virtual WebCore::Color backgroundColor() const override;
    virtual void setBackgroundColor(const WebCore::Color&) override;

    virtual void setBorderWidth(float) override;
    virtual void setBorderColor(const WebCore::Color&) override;

    virtual float opacity() const override;
    virtual void setOpacity(float) override;

#if ENABLE(CSS_FILTERS)
    virtual void setFilters(const WebCore::FilterOperations&) override;
    static bool filtersCanBeComposited(const WebCore::FilterOperations&);
    virtual void copyFiltersFrom(const WebCore::PlatformCALayer*) override;
#endif

    virtual void setName(const String&) override;

    virtual void setSpeed(float) override;

    virtual void setTimeOffset(CFTimeInterval) override;

    virtual float contentsScale() const override;
    virtual void setContentsScale(float) override;

    virtual void setEdgeAntialiasingMask(unsigned) override;

    virtual WebCore::GraphicsLayer::CustomAppearance customAppearance() const override;
    virtual void updateCustomAppearance(WebCore::GraphicsLayer::CustomAppearance) override;

    virtual WebCore::GraphicsLayer::CustomBehavior customBehavior() const override;
    virtual void updateCustomBehavior(WebCore::GraphicsLayer::CustomBehavior) override;

    virtual WebCore::TiledBacking* tiledBacking() override { return nullptr; }

    virtual PassRefPtr<WebCore::PlatformCALayer> clone(WebCore::PlatformCALayerClient* owner) const override;

    virtual PassRefPtr<PlatformCALayer> createCompatibleLayer(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*) const override;

    virtual void enumerateRectsBeingDrawn(CGContextRef, void (^block)(CGRect)) override;

    virtual uint32_t hostingContextID();

protected:
    PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext* context);
    PlatformCALayerRemote(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext*);

private:
    virtual bool isPlatformCALayerRemote() const override { return true; }
    void ensureBackingStore();
    void updateBackingStore();
    void removeSublayer(PlatformCALayerRemote*);

    bool requiresCustomAppearanceUpdateOnBoundsChange() const;

    RemoteLayerTreeTransaction::LayerProperties m_properties;
    WebCore::PlatformCALayerList m_children;
    PlatformCALayerRemote* m_superlayer;
    PlatformCALayerRemote* m_maskLayer;
    bool m_acceleratesDrawing;

    RemoteLayerTreeContext* m_context;
};

PLATFORM_CALAYER_TYPE_CASTS(PlatformCALayerRemote, isPlatformCALayerRemote())

} // namespace WebKit

#endif // PlatformCALayerRemote_h
