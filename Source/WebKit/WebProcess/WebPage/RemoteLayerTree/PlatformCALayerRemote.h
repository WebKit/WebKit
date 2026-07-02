/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "LayerProperties.h"
#include "RemoteLayerTreeContext.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformCALayerDelegatedContents.h>
#include <WebCore/PlatformLayer.h>
#include <wtf/MachSendRightAnnotated.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class LayerPool;
#if ENABLE(THREADED_ANIMATIONS)
class AcceleratedEffect;
struct AcceleratedEffectValues;
#endif
#if ENABLE(MODEL_PROCESS)
class ModelContext;
#endif
enum class GraphicsLayerCustomAppearance : bool;
}

namespace WebKit {

using LayerHostingContextID = uint32_t;

struct PlatformCALayerRemoteDelegatedContents {
    ImageBufferBackendHandle surface;
    RefPtr<WebCore::PlatformCALayerDelegatedContentsFence> finishedFence;
    std::optional<WebCore::RenderingResourceIdentifier> surfaceIdentifier;
};

class PlatformCALayerRemote : public WebCore::PlatformCALayer, public CanMakeWeakPtr<PlatformCALayerRemote> {
public:
    static Ref<PlatformCALayerRemote> create(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
    static Ref<PlatformCALayerRemote> create(PlatformLayer *, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
#if ENABLE(MODEL_PROCESS)
    static Ref<PlatformCALayerRemote> create(Ref<WebCore::ModelContext>, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);
#endif
#if ENABLE(MODEL_ELEMENT)
    static Ref<PlatformCALayerRemote> create(Ref<WebCore::Model>, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
#endif
#if HAVE(AVKIT)
    static Ref<PlatformCALayerRemote> create(WebCore::HTMLVideoElement&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);
#endif
    static Ref<PlatformCALayerRemote> create(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);

    virtual ~PlatformCALayerRemote();

    PlatformLayer* platformLayer() const override { return nullptr; }

    void recursiveBuildTransaction(RemoteLayerTreeContext&, RemoteLayerTreeTransaction&);
    void recursiveMarkWillBeDisplayedWithRenderingSuppresion();

    void setNeedsDisplayInRect(const WebCore::FloatRect& dirtyRect) override;
    void setNeedsDisplay() override;
    bool needsDisplay() const override;

    void copyContentsFromLayer(PlatformCALayer*) override;

    WebCore::PlatformCALayer* NODELETE superlayer() const override;
    void removeFromSuperlayer() override;
    void setSublayers(const WebCore::PlatformCALayerList&) override;
    WebCore::PlatformCALayerList sublayersForLogging() const override { return m_children; }
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

#if ENABLE(THREADED_ANIMATIONS)
    void clearAcceleratedEffectsAndBaseValues() override;
    void setAcceleratedEffectsAndBaseValues(const WebCore::AcceleratedEffects&, const WebCore::AcceleratedEffectValues&) override;
#endif

    void setMaskLayer(RefPtr<WebCore::PlatformCALayer>&&) override;

    bool NODELETE isOpaque() const override;
    void setOpaque(bool) override;

    WebCore::FloatRect NODELETE bounds() const override;
    void setBounds(const WebCore::FloatRect&) override;

    WebCore::FloatPoint3D NODELETE position() const override;
    void NODELETE setPosition(const WebCore::FloatPoint3D&) override;

    WebCore::FloatPoint3D NODELETE anchorPoint() const override;
    void NODELETE setAnchorPoint(const WebCore::FloatPoint3D&) override;

    WebCore::TransformationMatrix NODELETE transform() const override;
    void setTransform(const WebCore::TransformationMatrix&) override;

    WebCore::TransformationMatrix NODELETE sublayerTransform() const override;
    void setSublayerTransform(const WebCore::TransformationMatrix&) override;

    void NODELETE setIsBackdropRoot(bool) final;
    bool NODELETE backdropRootIsOpaque() const final;
    void NODELETE setBackdropRootIsOpaque(bool) final;

    bool NODELETE isHidden() const override;
    void NODELETE setHidden(bool) override;

    bool NODELETE contentsHidden() const override;
    void NODELETE setContentsHidden(bool) override;

    bool NODELETE userInteractionEnabled() const override;
    void NODELETE setUserInteractionEnabled(bool) override;

    void setBackingStoreAttached(bool) override;
    bool NODELETE backingStoreAttached() const override;

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    void setVisibleRect(const WebCore::FloatRect&) override;
#endif

    bool NODELETE geometryFlipped() const override;
    void NODELETE setGeometryFlipped(bool) override;

    bool NODELETE isDoubleSided() const override;
    void NODELETE setDoubleSided(bool) override;

    bool NODELETE masksToBounds() const override;
    void NODELETE setMasksToBounds(bool) override;

    bool NODELETE acceleratesDrawing() const override;
    void setAcceleratesDrawing(bool) override;

    WebCore::ContentsFormat NODELETE contentsFormat() const override;
    void setContentsFormat(WebCore::ContentsFormat) override;

    bool NODELETE hasContents() const override;
    CFTypeRef contents() const override;
    void setContents(CFTypeRef) override;
    void setDelegatedContents(const WebCore::PlatformCALayerDelegatedContents&) override;
    void setRemoteDelegatedContents(const PlatformCALayerRemoteDelegatedContents&);
    void NODELETE setContentsRect(const WebCore::FloatRect&) override;

    void NODELETE setMinificationFilter(WebCore::PlatformCALayer::FilterType) override;
    void NODELETE setMagnificationFilter(WebCore::PlatformCALayer::FilterType) override;

    WebCore::Color NODELETE backgroundColor() const override;
    void setBackgroundColor(const WebCore::Color&) override;

    void setBorderWidth(float) override;
    void setBorderColor(const WebCore::Color&) override;

    float NODELETE opacity() const override;
    void NODELETE setOpacity(float) override;

    void setFilters(const WebCore::FilterOperations&) override;
    static bool filtersCanBeComposited(const WebCore::FilterOperations&);
    void copyFiltersFrom(const WebCore::PlatformCALayer&) override;

    void NODELETE setBlendMode(WebCore::BlendMode) override;

    void setName(const String&) override;

    void NODELETE setSpeed(float) override;

    void NODELETE setTimeOffset(CFTimeInterval) override;

    float NODELETE contentsScale() const override;
    void setContentsScale(float) override;

    float NODELETE cornerRadius() const override;
    void NODELETE setCornerRadius(float) override;

    WebCore::Path shadowPath() const override;
    void setShadowPath(const WebCore::Path&) override;

    void NODELETE setAntialiasesEdges(bool) override;

    WebCore::MediaPlayerVideoGravity NODELETE videoGravity() const override;
    void NODELETE setVideoGravity(WebCore::MediaPlayerVideoGravity) override;

    // FIXME: Having both shapeRoundedRect and shapePath is redundant. We could use shapePath for everything.
    WebCore::FloatRoundedRect shapeRoundedRect() const override;
    void setShapeRoundedRect(const WebCore::FloatRoundedRect&) override;

    WebCore::Path shapePath() const override;
    void setShapePath(const WebCore::Path&) override;

    WebCore::WindRule NODELETE shapeWindRule() const override;
    void NODELETE setShapeWindRule(WebCore::WindRule) override;

    WebCore::GraphicsLayerCustomAppearance NODELETE customAppearance() const override;
    void updateCustomAppearance(WebCore::GraphicsLayerCustomAppearance) override;

    void setEventRegion(const WebCore::EventRegion&) override;

#if ENABLE(SCROLLING_THREAD)
    std::optional<WebCore::ScrollingNodeID> NODELETE scrollingNodeID() const override;
    void NODELETE setScrollingNodeID(std::optional<WebCore::ScrollingNodeID>) override;
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
    bool setNeedsDisplayIfEDRHeadroomExceeds(float) override;
    void setTonemappingEnabled(bool) override;
    bool NODELETE tonemappingEnabled() const override;
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

#if HAVE(CORE_MATERIAL)
    WebCore::AppleVisualEffectData NODELETE appleVisualEffectData() const override;
    void NODELETE setAppleVisualEffectData(WebCore::AppleVisualEffectData) override;
#endif

    WebCore::TiledBacking* tiledBacking() override { return nullptr; }

    Ref<WebCore::PlatformCALayer> clone(WebCore::PlatformCALayerClient* owner) const override;

    Ref<PlatformCALayer> createCompatibleLayer(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient*) const override;

    void enumerateRectsBeingDrawn(WebCore::GraphicsContext&, void (^block)(WebCore::FloatRect)) override;

    virtual uint32_t NODELETE hostingContextID();

    unsigned NODELETE backingStoreBytesPerPixel() const override;

    void NODELETE setClonedLayer(const PlatformCALayer*);

    LayerProperties& properties() LIFETIME_BOUND { return m_properties; }
    const LayerProperties& properties() const LIFETIME_BOUND { return m_properties; }

    void didCommit();

    void moveToContext(RemoteLayerTreeContext&);
    RemoteLayerTreeContext* context() const { return m_context.get(); }

    void markFrontBufferVolatileForTesting() override;
    virtual void populateCreationProperties(RemoteLayerTreeTransaction::LayerCreationProperties&, const RemoteLayerTreeContext&, WebCore::PlatformCALayer::LayerType);

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    bool allowsDynamicContentScaling() const;
#endif

    void purgeFrontBufferForTesting() override;
    void purgeBackBufferForTesting() override;

#if ENABLE(MACH_PORT_LAYER_HOSTING)
    void setSendRightAnnotated(WTF::MachSendRightAnnotated sendRightAnnotated) { m_sendRightAnnotated = sendRightAnnotated; }
    std::optional<WTF::MachSendRightAnnotated> sendRightAnnotated() const { return m_sendRightAnnotated; }
#endif

protected:
    PlatformCALayerRemote(WebCore::PlatformCALayer::LayerType, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext&);
    PlatformCALayerRemote(const PlatformCALayerRemote&, WebCore::PlatformCALayerClient*, RemoteLayerTreeContext&);

    void updateClonedLayerProperties(PlatformCALayerRemote& clone, bool copyContents = true) const;

private:
    Type type() const override { return Type::Remote; }
    void ensureBackingStore();
    void updateBackingStore();
    void removeSublayer(PlatformCALayerRemote*);

    WebCore::DestinationColorSpace displayColorSpace() const;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::IncludeDynamicContentScalingDisplayList shouldIncludeDisplayListInBackingStore() const;
#endif

    bool NODELETE requiresCustomAppearanceUpdateOnBoundsChange() const;

    WebCore::LayerPool* NODELETE layerPool() override;

    LayerProperties m_properties;
    WebCore::PlatformCALayerList m_children;
    WeakPtr<PlatformCALayerRemote> m_superlayer;
    HashMap<String, Ref<WebCore::PlatformCAAnimation>> m_animations;

    bool m_acceleratesDrawing { false };
    WeakPtr<RemoteLayerTreeContext> m_context;

#if ENABLE(MACH_PORT_LAYER_HOSTING)
    std::optional<WTF::MachSendRightAnnotated> m_sendRightAnnotated;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::PlatformCALayerRemote)
static bool isType(const WebCore::PlatformCALayer& layer)
{
    switch (layer.type()) {
    case WebCore::PlatformCALayer::Type::Cocoa:
        break;
    case WebCore::PlatformCALayer::Type::Remote:
    case WebCore::PlatformCALayer::Type::RemoteCustom:
    case WebCore::PlatformCALayer::Type::RemoteHost:
    case WebCore::PlatformCALayer::Type::RemoteModel:
        return true;
    };
    return false;
}
SPECIALIZE_TYPE_TRAITS_END()
