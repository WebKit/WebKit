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

#pragma once

#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformLayer.h>

namespace WebCore {
class LayerPool;
}

namespace WebKit {

class RemoteLayerTreeContext;

class PlatformCALayerRemote : public WebCore::PlatformCALayer {
public:
    static Ref<PlatformCALayerRemote> create(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
    static Ref<PlatformCALayerRemote> create(PlatformLayer *, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
    static Ref<PlatformCALayerRemote> create(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
    static Ref<PlatformCALayerRemote> createForEmbeddedView(WebCore::PlatformCALayer::LayerType, WebCore::GraphicsLayer::EmbeddedViewID, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);

    virtual ~PlatformCALayerRemote();

    PlatformLayer* platformLayer() const override { return nullptr; }

    void recursiveBuildTransaction(RemoteLayerTreeContext&, RemoteLayerTreeTransaction&);

    void setNeedsDisplayInRect(const WebCore::FloatRect& dirtyRect) override;
    void setNeedsDisplay() override;

    void copyContentsFromLayer(PlatformCALayer*) override;

    WebCore::PlatformCALayer* superlayer() const override;
    void removeFromSuperlayer() override;
    void setSublayers(const WebCore::PlatformCALayerList&) override;
    void removeAllSublayers() override;
    void appendSublayer(WebCore::PlatformCALayer&) override;
    void insertSublayer(WebCore::PlatformCALayer&, size_t index) override;
    void replaceSublayer(WebCore::PlatformCALayer& reference, WebCore::PlatformCALayer&) override;
    const WebCore::PlatformCALayerList* customSublayers() const override { return nullptr; }
    void adoptSublayers(WebCore::PlatformCALayer& source) override;

    void addAnimationForKey(const String& key, WebCore::PlatformCAAnimation&) override;
    void removeAnimationForKey(const String& key) override;
    RefPtr<WebCore::PlatformCAAnimation> animationForKey(const String& key) override;
    void animationStarted(const String& key, MonotonicTime beginTime) override;
    void animationEnded(const String& key) override;

    void setMask(WebCore::PlatformCALayer*) override;

    bool isOpaque() const override;
    void setOpaque(bool) override;

    WebCore::FloatRect bounds() const override;
    void setBounds(const WebCore::FloatRect&) override;

    WebCore::FloatPoint3D position() const override;
    void setPosition(const WebCore::FloatPoint3D&) override;

    WebCore::FloatPoint3D anchorPoint() const override;
    void setAnchorPoint(const WebCore::FloatPoint3D&) override;

    WebCore::TransformationMatrix transform() const override;
    void setTransform(const WebCore::TransformationMatrix&) override;

    WebCore::TransformationMatrix sublayerTransform() const override;
    void setSublayerTransform(const WebCore::TransformationMatrix&) override;

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

    void setContentsRect(const WebCore::FloatRect&) override;

    void setMinificationFilter(WebCore::PlatformCALayer::FilterType) override;
    void setMagnificationFilter(WebCore::PlatformCALayer::FilterType) override;

    WebCore::Color backgroundColor() const override;
    void setBackgroundColor(const WebCore::Color&) override;

    void setBorderWidth(float) override;
    void setBorderColor(const WebCore::Color&) override;

    float opacity() const override;
    void setOpacity(float) override;

    void setFilters(const WebCore::FilterOperations&) override;
    static bool filtersCanBeComposited(const WebCore::FilterOperations&);
    void copyFiltersFrom(const WebCore::PlatformCALayer&) override;

#if ENABLE(CSS_COMPOSITING)
    void setBlendMode(WebCore::BlendMode) override;
#endif

    void setName(const String&) override;

    void setSpeed(float) override;

    void setTimeOffset(CFTimeInterval) override;

    float contentsScale() const override;
    void setContentsScale(float) override;

    float cornerRadius() const override;
    void setCornerRadius(float) override;

    void setEdgeAntialiasingMask(unsigned) override;

    // FIXME: Having both shapeRoundedRect and shapePath is redundant. We could use shapePath for everything.
    WebCore::FloatRoundedRect shapeRoundedRect() const override;
    void setShapeRoundedRect(const WebCore::FloatRoundedRect&) override;

    WebCore::Path shapePath() const override;
    void setShapePath(const WebCore::Path&) override;

    WebCore::WindRule shapeWindRule() const override;
    void setShapeWindRule(WebCore::WindRule) override;

    WebCore::GraphicsLayer::CustomAppearance customAppearance() const override;
    void updateCustomAppearance(WebCore::GraphicsLayer::CustomAppearance) override;

    void setEventRegion(const WebCore::EventRegion&) override;

    WebCore::GraphicsLayer::EmbeddedViewID embeddedViewID() const override;

    WebCore::TiledBacking* tiledBacking() override { return nullptr; }

    Ref<WebCore::PlatformCALayer> clone(WebCore::PlatformCALayerClient* owner) const override;

    Ref<PlatformCALayer> createCompatibleLayer(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*) const override;

    void enumerateRectsBeingDrawn(CGContextRef, void (^block)(CGRect)) override;

    virtual uint32_t hostingContextID();

    unsigned backingStoreBytesPerPixel() const override;

    void setClonedLayer(const PlatformCALayer*);

    RemoteLayerTreeTransaction::LayerProperties& properties() { return m_properties; }
    const RemoteLayerTreeTransaction::LayerProperties& properties() const { return m_properties; }

    void didCommit();

    void moveToContext(RemoteLayerTreeContext&);
    void clearContext() { m_context = nullptr; }
    RemoteLayerTreeContext* context() const { return m_context; }

protected:
    PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);
    PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType, WebCore::GraphicsLayer::EmbeddedViewID, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);
    PlatformCALayerRemote(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);

    void updateClonedLayerProperties(PlatformCALayerRemote& clone, bool copyContents = true) const;

private:
    bool isPlatformCALayerRemote() const override { return true; }
    void ensureBackingStore();
    void updateBackingStore();
    void removeSublayer(PlatformCALayerRemote*);

    bool requiresCustomAppearanceUpdateOnBoundsChange() const;

    WebCore::LayerPool& layerPool() override;

    RemoteLayerTreeTransaction::LayerProperties m_properties;
    WebCore::PlatformCALayerList m_children;
    PlatformCALayerRemote* m_superlayer { nullptr };
    PlatformCALayerRemote* m_maskLayer { nullptr };
    HashMap<String, RefPtr<WebCore::PlatformCAAnimation>> m_animations;

    WebCore::GraphicsLayer::EmbeddedViewID m_embeddedViewID;

    bool m_acceleratesDrawing { false };
    bool m_wantsDeepColorBackingStore { false };

    RemoteLayerTreeContext* m_context;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_PLATFORM_CALAYER(WebKit::PlatformCALayerRemote, isPlatformCALayerRemote())
