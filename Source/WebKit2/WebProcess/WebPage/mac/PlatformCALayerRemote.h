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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#if USE(ACCELERATED_COMPOSITING)

#include "RemoteLayerTreeTransaction.h"
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformLayer.h>

namespace WebKit {

class RemoteLayerTreeContext;

class PlatformCALayerRemote FINAL : public WebCore::PlatformCALayer {
public:
    static PassRefPtr<PlatformCALayer> create(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext*);

    ~PlatformCALayerRemote();

    RemoteLayerTreeTransaction::LayerID layerID() { return m_layerID; }

    virtual bool isRemote() const OVERRIDE { return true; }

    virtual bool usesTiledBackingLayer() const OVERRIDE { return false; }

    virtual PlatformLayer* platformLayer() const OVERRIDE { return nullptr; }

    void recursiveBuildTransaction(RemoteLayerTreeTransaction&);

    virtual void setNeedsDisplay(const WebCore::FloatRect* dirtyRect = 0) OVERRIDE;

    virtual void setContentsChanged() OVERRIDE;

    virtual WebCore::PlatformCALayer* superlayer() const OVERRIDE;
    virtual void removeFromSuperlayer() OVERRIDE;
    virtual void setSublayers(const WebCore::PlatformCALayerList&) OVERRIDE;
    virtual void removeAllSublayers() OVERRIDE;
    virtual void appendSublayer(WebCore::PlatformCALayer*) OVERRIDE;
    virtual void insertSublayer(WebCore::PlatformCALayer*, size_t index) OVERRIDE;
    virtual void replaceSublayer(WebCore::PlatformCALayer* reference, WebCore::PlatformCALayer*) OVERRIDE;
    virtual const WebCore::PlatformCALayerList* customSublayers() const OVERRIDE { return nullptr; }
    virtual void adoptSublayers(WebCore::PlatformCALayer* source) OVERRIDE;

    virtual void addAnimationForKey(const String& key, WebCore::PlatformCAAnimation*) OVERRIDE;
    virtual void removeAnimationForKey(const String& key) OVERRIDE;
    virtual PassRefPtr<WebCore::PlatformCAAnimation> animationForKey(const String& key) OVERRIDE;
    virtual void animationStarted(CFTimeInterval beginTime) OVERRIDE;

    virtual void setMask(WebCore::PlatformCALayer*) OVERRIDE;

    virtual bool isOpaque() const OVERRIDE;
    virtual void setOpaque(bool) OVERRIDE;

    virtual WebCore::FloatRect bounds() const OVERRIDE;
    virtual void setBounds(const WebCore::FloatRect&) OVERRIDE;

    virtual WebCore::FloatPoint3D position() const OVERRIDE;
    virtual void setPosition(const WebCore::FloatPoint3D&) OVERRIDE;

    virtual WebCore::FloatPoint3D anchorPoint() const OVERRIDE;
    virtual void setAnchorPoint(const WebCore::FloatPoint3D&) OVERRIDE;

    virtual WebCore::TransformationMatrix transform() const OVERRIDE;
    virtual void setTransform(const WebCore::TransformationMatrix&) OVERRIDE;

    virtual WebCore::TransformationMatrix sublayerTransform() const OVERRIDE;
    virtual void setSublayerTransform(const WebCore::TransformationMatrix&) OVERRIDE;

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

    virtual void setContentsRect(const WebCore::FloatRect&) OVERRIDE;

    virtual void setMinificationFilter(WebCore::PlatformCALayer::FilterType) OVERRIDE;
    virtual void setMagnificationFilter(WebCore::PlatformCALayer::FilterType) OVERRIDE;

    virtual WebCore::Color backgroundColor() const OVERRIDE;
    virtual void setBackgroundColor(const WebCore::Color&) OVERRIDE;

    virtual void setBorderWidth(float) OVERRIDE;

    virtual void setBorderColor(const WebCore::Color&) OVERRIDE;

    virtual float opacity() const OVERRIDE;
    virtual void setOpacity(float) OVERRIDE;

#if ENABLE(CSS_FILTERS)
    virtual void setFilters(const WebCore::FilterOperations&) OVERRIDE;
    static bool filtersCanBeComposited(const WebCore::FilterOperations&);
    virtual void copyFiltersFrom(const WebCore::PlatformCALayer*) OVERRIDE;
#endif

    virtual void setName(const String&) OVERRIDE;

    virtual void setSpeed(float) OVERRIDE;

    virtual void setTimeOffset(CFTimeInterval) OVERRIDE;

    virtual float contentsScale() const OVERRIDE;
    virtual void setContentsScale(float) OVERRIDE;

    virtual WebCore::TiledBacking* tiledBacking() OVERRIDE;

    virtual void synchronouslyDisplayTilesInRect(const WebCore::FloatRect&) OVERRIDE;

    virtual PassRefPtr<WebCore::PlatformCALayer> clone(WebCore::PlatformCALayerClient* owner) const OVERRIDE;

private:
    PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext* context);

    virtual AVPlayerLayer *playerLayer() const OVERRIDE;

    void ensureBackingStore();
    void removeSublayer(PlatformCALayerRemote*);

    RemoteLayerTreeTransaction::LayerID m_layerID;
    RemoteLayerTreeTransaction::LayerProperties m_properties;
    WebCore::PlatformCALayerList m_children;
    PlatformCALayerRemote* m_superlayer;

    RemoteLayerTreeContext* m_context;
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)

#endif // PlatformCALayerRemote_h
