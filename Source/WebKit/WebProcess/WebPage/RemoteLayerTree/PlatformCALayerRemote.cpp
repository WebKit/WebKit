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

#include "config.h"
#import "PlatformCALayerRemote.h"

#import "PlatformCALayerRemoteCustom.h"
#import "PlatformCALayerRemoteTiledBacking.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import <WebCore/AnimationUtilities.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/PlatformCALayerCocoa.h>
#import <WebCore/TiledBacking.h>
#import <wtf/PointerComparison.h>

namespace WebKit {
using namespace WebCore;

Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    RefPtr<PlatformCALayerRemote> layer;

    if (layerType == LayerTypeTiledBackingLayer ||  layerType == LayerTypePageTiledBackingLayer)
        layer = adoptRef(new PlatformCALayerRemoteTiledBacking(layerType, owner, context));
    else
        layer = adoptRef(new PlatformCALayerRemote(layerType, owner, context));

    context.layerDidEnterContext(*layer, layerType);

    return layer.releaseNonNull();
}

Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(PlatformLayer *platformLayer, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    return PlatformCALayerRemoteCustom::create(platformLayer, owner, context);
}

Ref<PlatformCALayerRemote> PlatformCALayerRemote::createForEmbeddedView(LayerType layerType, GraphicsLayer::EmbeddedViewID embeddedViewID, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    RefPtr<PlatformCALayerRemote> layer = adoptRef(new PlatformCALayerRemote(layerType, embeddedViewID, owner, context));
    context.layerDidEnterContext(*layer, layerType);
    return layer.releaseNonNull();
}

Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(const PlatformCALayerRemote& other, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    auto layer = adoptRef(*new PlatformCALayerRemote(other, owner, context));

    context.layerDidEnterContext(layer.get(), other.layerType());

    return layer;
}

PlatformCALayerRemote::PlatformCALayerRemote(LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayer(layerType, owner)
    , m_context(&context)
{
    if (owner && layerType != LayerTypeContentsProvidedLayer && layerType != LayerTypeTransformLayer) {
        m_properties.contentsScale = owner->platformCALayerDeviceScaleFactor();
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsScaleChanged);
    }
}

PlatformCALayerRemote::PlatformCALayerRemote(LayerType layerType, GraphicsLayer::EmbeddedViewID embeddedViewID, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayerRemote(layerType, owner, context)
{
    m_embeddedViewID = embeddedViewID;
}

PlatformCALayerRemote::PlatformCALayerRemote(const PlatformCALayerRemote& other, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayer(other.layerType(), owner)
    , m_acceleratesDrawing(other.acceleratesDrawing())
    , m_context(&context)
{
}

Ref<PlatformCALayer> PlatformCALayerRemote::clone(PlatformCALayerClient* owner) const
{
    auto clone = PlatformCALayerRemote::create(*this, owner, *m_context);

    updateClonedLayerProperties(clone);

    clone->setClonedLayer(this);
    return WTFMove(clone);
}

PlatformCALayerRemote::~PlatformCALayerRemote()
{
    for (const auto& layer : m_children)
        downcast<PlatformCALayerRemote>(*layer).m_superlayer = nullptr;

    if (m_context)
        m_context->layerWillLeaveContext(*this);
}

void PlatformCALayerRemote::moveToContext(RemoteLayerTreeContext& context)
{
    if (m_context)
        m_context->layerWillLeaveContext(*this);

    m_context = &context;

    context.layerDidEnterContext(*this, layerType());

    m_properties.notePropertiesChanged(m_properties.everChangedProperties);
}

void PlatformCALayerRemote::updateClonedLayerProperties(PlatformCALayerRemote& clone, bool copyContents) const
{
    clone.setPosition(position());
    clone.setBounds(bounds());
    clone.setAnchorPoint(anchorPoint());

    if (m_properties.transform)
        clone.setTransform(*m_properties.transform);

    if (m_properties.sublayerTransform)
        clone.setSublayerTransform(*m_properties.sublayerTransform);

    if (copyContents)
        clone.setContents(contents());

    clone.setMasksToBounds(masksToBounds());
    clone.setDoubleSided(isDoubleSided());
    clone.setOpaque(isOpaque());
    clone.setBackgroundColor(backgroundColor());
    clone.setContentsScale(contentsScale());
    clone.setCornerRadius(cornerRadius());

    if (!m_properties.shapePath.isNull())
        clone.setShapePath(m_properties.shapePath);

    if (m_properties.shapeRoundedRect)
        clone.setShapeRoundedRect(*m_properties.shapeRoundedRect);

    if (m_properties.filters)
        clone.copyFiltersFrom(*this);

    clone.updateCustomAppearance(customAppearance());
}

void PlatformCALayerRemote::recursiveBuildTransaction(RemoteLayerTreeContext& context, RemoteLayerTreeTransaction& transaction)
{
    ASSERT(!m_properties.backingStore || owner());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&context == m_context);
    
    if (m_properties.backingStore && (!owner() || !owner()->platformCALayerDrawsContent())) {
        m_properties.backingStore = nullptr;
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackingStoreChanged);
    }

    if (m_properties.backingStore && m_properties.backingStoreAttached && m_properties.backingStore->display())
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackingStoreChanged);

    if (m_properties.changedProperties) {
        if (m_properties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged) {
            m_properties.children.resize(m_children.size());
            for (size_t i = 0; i < m_children.size(); ++i)
                m_properties.children[i] = m_children[i]->layerID();
        }

        if (isPlatformCALayerRemoteCustom()) {
            RemoteLayerTreePropertyApplier::applyPropertiesToLayer(platformLayer(), nullptr, m_properties, RemoteLayerBackingStore::LayerContentsType::CAMachPort);
            didCommit();
            return;
        }

        transaction.layerPropertiesChanged(*this);
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        PlatformCALayerRemote& child = downcast<PlatformCALayerRemote>(*m_children[i]);
        ASSERT(child.superlayer() == this);
        child.recursiveBuildTransaction(context, transaction);
    }

    if (m_maskLayer)
        m_maskLayer->recursiveBuildTransaction(context, transaction);
}

void PlatformCALayerRemote::didCommit()
{
    m_properties.addedAnimations.clear();
    m_properties.keyPathsOfAnimationsToRemove.clear();
    m_properties.resetChangedProperties();
}

void PlatformCALayerRemote::ensureBackingStore()
{
    ASSERT(owner());

    ASSERT(m_properties.backingStoreAttached);

    if (!m_properties.backingStore)
        m_properties.backingStore = std::make_unique<RemoteLayerBackingStore>(this);

    updateBackingStore();
}

void PlatformCALayerRemote::updateBackingStore()
{
    if (!m_properties.backingStore)
        return;

    ASSERT(m_properties.backingStoreAttached);

    m_properties.backingStore->ensureBackingStore(m_properties.bounds.size(), m_properties.contentsScale, m_acceleratesDrawing, m_wantsDeepColorBackingStore, m_properties.opaque);
}

void PlatformCALayerRemote::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (!m_properties.backingStoreAttached)
        return;

    ensureBackingStore();

    // FIXME: Need to map this through contentsRect/etc.
    m_properties.backingStore->setNeedsDisplay(enclosingIntRect(rect));
}

void PlatformCALayerRemote::setNeedsDisplay()
{
    if (!m_properties.backingStoreAttached)
        return;

    ensureBackingStore();

    m_properties.backingStore->setNeedsDisplay();
}

void PlatformCALayerRemote::copyContentsFromLayer(PlatformCALayer* layer)
{
    ASSERT(m_properties.clonedLayerID == layer->layerID());
    
    if (!m_properties.changedProperties)
        m_context->layerPropertyChangedWhileBuildingTransaction(*this);

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ClonedContentsChanged);
}

PlatformCALayer* PlatformCALayerRemote::superlayer() const
{
    return m_superlayer;
}

void PlatformCALayerRemote::removeFromSuperlayer()
{
    if (!m_superlayer)
        return;

    m_superlayer->removeSublayer(this);
}

void PlatformCALayerRemote::removeSublayer(PlatformCALayerRemote* layer)
{
    size_t childIndex = m_children.find(layer);
    if (childIndex != notFound)
        m_children.remove(childIndex);
    layer->m_superlayer = nullptr;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::setSublayers(const PlatformCALayerList& list)
{
    removeAllSublayers();
    m_children = list;

    for (const auto& layer : list) {
        layer->removeFromSuperlayer();
        downcast<PlatformCALayerRemote>(*layer).m_superlayer = this;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::removeAllSublayers()
{
    PlatformCALayerList layersToRemove = m_children;
    for (const auto& layer : layersToRemove)
        layer->removeFromSuperlayer();
    ASSERT(m_children.isEmpty());
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::appendSublayer(PlatformCALayer& layer)
{
    Ref<PlatformCALayer> protectedLayer(layer);

    layer.removeFromSuperlayer();
    m_children.append(&layer);
    downcast<PlatformCALayerRemote>(layer).m_superlayer = this;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::insertSublayer(PlatformCALayer& layer, size_t index)
{
    Ref<PlatformCALayer> protectedLayer(layer);

    layer.removeFromSuperlayer();
    m_children.insert(index, &layer);
    downcast<PlatformCALayerRemote>(layer).m_superlayer = this;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::replaceSublayer(PlatformCALayer& reference, PlatformCALayer& layer)
{
    ASSERT(reference.superlayer() == this);
    Ref<PlatformCALayer> protectedLayer(layer);

    layer.removeFromSuperlayer();
    size_t referenceIndex = m_children.find(&reference);
    if (referenceIndex != notFound) {
        m_children[referenceIndex]->removeFromSuperlayer();
        m_children.insert(referenceIndex, &layer);
        downcast<PlatformCALayerRemote>(layer).m_superlayer = this;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ChildrenChanged);
}

void PlatformCALayerRemote::adoptSublayers(PlatformCALayer& source)
{
    PlatformCALayerList layersToMove = downcast<PlatformCALayerRemote>(source).m_children;

    if (const PlatformCALayerList* customLayers = source.customSublayers()) {
        for (const auto& layer : *customLayers) {
            size_t layerIndex = layersToMove.find(layer);
            if (layerIndex != notFound)
                layersToMove.remove(layerIndex);
        }
    }

    setSublayers(layersToMove);
}

void PlatformCALayerRemote::addAnimationForKey(const String& key, PlatformCAAnimation& animation)
{
    auto addResult = m_animations.set(key, &animation);
    bool appendToAddedAnimations = true;
    if (!addResult.isNewEntry) {
        // There is already an animation for this key. If the animation has not been sent to the UI
        // process yet, we update the key properties before it happens. Otherwise, we just append it
        // to the list of animations to be added: PlatformCAAnimationRemote::updateLayerAnimations
        // will actually take care of replacing the existing animation.
        for (auto& keyAnimationPair : m_properties.addedAnimations) {
            if (keyAnimationPair.first == key) {
                keyAnimationPair.second = downcast<PlatformCAAnimationRemote>(animation).properties();
                appendToAddedAnimations = false;
                break;
            }
        }
    }
    if (appendToAddedAnimations)
        m_properties.addedAnimations.append(std::pair<String, PlatformCAAnimationRemote::Properties>(key, downcast<PlatformCAAnimationRemote>(animation).properties()));
    
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::AnimationsChanged);

    if (m_context)
        m_context->willStartAnimationOnLayer(*this);
}

void PlatformCALayerRemote::removeAnimationForKey(const String& key)
{
    if (m_animations.remove(key)) {
        m_properties.addedAnimations.removeFirstMatching([&key](auto& pair) {
            return pair.first == key;
        });
    }
    m_properties.keyPathsOfAnimationsToRemove.add(key);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::AnimationsChanged);
}

RefPtr<PlatformCAAnimation> PlatformCALayerRemote::animationForKey(const String& key)
{
    return m_animations.get(key);
}

static inline bool isEquivalentLayer(const PlatformCALayer* layer, GraphicsLayer::PlatformLayerID layerID)
{
    GraphicsLayer::PlatformLayerID newLayerID = layer ? layer->layerID() : 0;
    return layerID == newLayerID;
}

void PlatformCALayerRemote::animationStarted(const String& key, MonotonicTime beginTime)
{
    auto it = m_animations.find(key);
    if (it != m_animations.end())
        downcast<PlatformCAAnimationRemote>(*it->value).didStart(currentTimeToMediaTime(beginTime));
    
    if (m_owner)
        m_owner->platformCALayerAnimationStarted(key, beginTime);
}

void PlatformCALayerRemote::animationEnded(const String& key)
{
    if (m_owner)
        m_owner->platformCALayerAnimationEnded(key);
}

void PlatformCALayerRemote::setMask(PlatformCALayer* layer)
{
    if (isEquivalentLayer(layer, m_properties.maskLayerID))
        return;
    
    if (layer) {
        m_maskLayer = downcast<PlatformCALayerRemote>(layer);
        m_properties.maskLayerID = m_maskLayer->layerID();
    } else {
        m_maskLayer = nullptr;
        m_properties.maskLayerID = 0;
    }

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MaskLayerChanged);
}

void PlatformCALayerRemote::setClonedLayer(const PlatformCALayer* layer)
{
    if (isEquivalentLayer(layer, m_properties.clonedLayerID))
        return;

    if (layer)
        m_properties.clonedLayerID = layer->layerID();
    else
        m_properties.clonedLayerID = 0;

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ClonedContentsChanged);
}

bool PlatformCALayerRemote::isOpaque() const
{
    return m_properties.opaque;
}

void PlatformCALayerRemote::setOpaque(bool value)
{
    m_properties.opaque = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::OpaqueChanged);

    updateBackingStore();
}

FloatRect PlatformCALayerRemote::bounds() const
{
    return m_properties.bounds;
}

void PlatformCALayerRemote::setBounds(const FloatRect& value)
{
    if (value == m_properties.bounds)
        return;

    m_properties.bounds = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BoundsChanged);
    
    if (requiresCustomAppearanceUpdateOnBoundsChange())
        m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::CustomAppearanceChanged);

    updateBackingStore();
}

FloatPoint3D PlatformCALayerRemote::position() const
{
    return m_properties.position;
}

void PlatformCALayerRemote::setPosition(const FloatPoint3D& value)
{
    // We can't early return here if the position has not changed, since GraphicsLayerCA::syncPosition() may have changed
    // the GraphicsLayer position (which doesn't force a geometry update) but we want a subsequent GraphicsLayerCA::setPosition()
    // to push a new position to the UI process, even though our m_properties.position hasn't changed.
    m_properties.position = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::PositionChanged);
}

FloatPoint3D PlatformCALayerRemote::anchorPoint() const
{
    return m_properties.anchorPoint;
}

void PlatformCALayerRemote::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == m_properties.anchorPoint)
        return;

    m_properties.anchorPoint = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::AnchorPointChanged);
}

TransformationMatrix PlatformCALayerRemote::transform() const
{
    return m_properties.transform ? *m_properties.transform : TransformationMatrix();
}

void PlatformCALayerRemote::setTransform(const TransformationMatrix& value)
{
    m_properties.transform = std::make_unique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::TransformChanged);
}

TransformationMatrix PlatformCALayerRemote::sublayerTransform() const
{
    return m_properties.sublayerTransform ? *m_properties.sublayerTransform : TransformationMatrix();
}

void PlatformCALayerRemote::setSublayerTransform(const TransformationMatrix& value)
{
    m_properties.sublayerTransform = std::make_unique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SublayerTransformChanged);
}

bool PlatformCALayerRemote::isHidden() const
{
    return m_properties.hidden;
}

void PlatformCALayerRemote::setHidden(bool value)
{
    if (m_properties.hidden == value)
        return;

    m_properties.hidden = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::HiddenChanged);
}

bool PlatformCALayerRemote::contentsHidden() const
{
    return m_properties.contentsHidden;
}

void PlatformCALayerRemote::setContentsHidden(bool value)
{
    if (m_properties.contentsHidden == value)
        return;

    m_properties.contentsHidden = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsHiddenChanged);
}

bool PlatformCALayerRemote::userInteractionEnabled() const
{
    return m_properties.userInteractionEnabled;
}

void PlatformCALayerRemote::setUserInteractionEnabled(bool value)
{
    if (m_properties.userInteractionEnabled == value)
        return;
    
    m_properties.userInteractionEnabled = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::UserInteractionEnabledChanged);
}

void PlatformCALayerRemote::setBackingStoreAttached(bool attached)
{
    if (m_properties.backingStoreAttached == attached)
        return;

    m_properties.backingStoreAttached = attached;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackingStoreAttachmentChanged);
    
    if (attached)
        setNeedsDisplay();
    else
        m_properties.backingStore = nullptr;
}

bool PlatformCALayerRemote::backingStoreAttached() const
{
    return m_properties.backingStoreAttached;
}

void PlatformCALayerRemote::setGeometryFlipped(bool value)
{
    m_properties.geometryFlipped = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::GeometryFlippedChanged);
}

bool PlatformCALayerRemote::geometryFlipped() const
{
    return m_properties.geometryFlipped;
}

bool PlatformCALayerRemote::isDoubleSided() const
{
    return m_properties.doubleSided;
}

void PlatformCALayerRemote::setDoubleSided(bool value)
{
    m_properties.doubleSided = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::DoubleSidedChanged);
}

bool PlatformCALayerRemote::masksToBounds() const
{
    return m_properties.masksToBounds;
}

void PlatformCALayerRemote::setMasksToBounds(bool value)
{
    if (value == m_properties.masksToBounds)
        return;

    m_properties.masksToBounds = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MasksToBoundsChanged);
}

bool PlatformCALayerRemote::acceleratesDrawing() const
{
    return m_acceleratesDrawing;
}

void PlatformCALayerRemote::setAcceleratesDrawing(bool acceleratesDrawing)
{
    m_acceleratesDrawing = acceleratesDrawing;
    updateBackingStore();
}

bool PlatformCALayerRemote::wantsDeepColorBackingStore() const
{
    return m_wantsDeepColorBackingStore;
}

void PlatformCALayerRemote::setWantsDeepColorBackingStore(bool wantsDeepColorBackingStore)
{
    m_wantsDeepColorBackingStore = wantsDeepColorBackingStore;
    updateBackingStore();
}

bool PlatformCALayerRemote::supportsSubpixelAntialiasedText() const
{
    return false;
}

void PlatformCALayerRemote::setSupportsSubpixelAntialiasedText(bool)
{
}

bool PlatformCALayerRemote::hasContents() const
{
    return !!m_properties.backingStore;
}

CFTypeRef PlatformCALayerRemote::contents() const
{
    return nullptr;
}

void PlatformCALayerRemote::setContents(CFTypeRef value)
{
}

void PlatformCALayerRemote::setContentsRect(const FloatRect& value)
{
    m_properties.contentsRect = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsRectChanged);
}

void PlatformCALayerRemote::setMinificationFilter(FilterType value)
{
    m_properties.minificationFilter = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MinificationFilterChanged);
}

void PlatformCALayerRemote::setMagnificationFilter(FilterType value)
{
    m_properties.magnificationFilter = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::MagnificationFilterChanged);
}

Color PlatformCALayerRemote::backgroundColor() const
{
    return m_properties.backgroundColor;
}

void PlatformCALayerRemote::setBackgroundColor(const Color& value)
{
    m_properties.backgroundColor = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BackgroundColorChanged);
}

void PlatformCALayerRemote::setBorderWidth(float value)
{
    if (value == m_properties.borderWidth)
        return;

    m_properties.borderWidth = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BorderWidthChanged);
}

void PlatformCALayerRemote::setBorderColor(const Color& value)
{
    if (value == m_properties.borderColor)
        return;

    m_properties.borderColor = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BorderColorChanged);
}

float PlatformCALayerRemote::opacity() const
{
    return m_properties.opacity;
}

void PlatformCALayerRemote::setOpacity(float value)
{
    m_properties.opacity = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::OpacityChanged);
}

void PlatformCALayerRemote::setFilters(const FilterOperations& filters)
{
    m_properties.filters = std::make_unique<FilterOperations>(filters);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::FiltersChanged);
}

void PlatformCALayerRemote::copyFiltersFrom(const PlatformCALayer& sourceLayer)
{
    if (const FilterOperations* filters = downcast<PlatformCALayerRemote>(sourceLayer).m_properties.filters.get())
        setFilters(*filters);
    else if (m_properties.filters)
        m_properties.filters = nullptr;

    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::FiltersChanged);
}

#if ENABLE(CSS_COMPOSITING)
void PlatformCALayerRemote::setBlendMode(BlendMode blendMode)
{
    m_properties.blendMode = blendMode;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::BlendModeChanged);
}
#endif

bool PlatformCALayerRemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerCocoa::filtersCanBeComposited(filters);
}

void PlatformCALayerRemote::setName(const String& value)
{
    m_properties.name = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::NameChanged);
}

void PlatformCALayerRemote::setSpeed(float value)
{
    m_properties.speed = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::SpeedChanged);
}

void PlatformCALayerRemote::setTimeOffset(CFTimeInterval value)
{
    m_properties.timeOffset = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::TimeOffsetChanged);
}

float PlatformCALayerRemote::contentsScale() const
{
    return m_properties.contentsScale;
}

void PlatformCALayerRemote::setContentsScale(float value)
{
    m_properties.contentsScale = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ContentsScaleChanged);

    updateBackingStore();
}

float PlatformCALayerRemote::cornerRadius() const
{
    return m_properties.cornerRadius;
}

void PlatformCALayerRemote::setCornerRadius(float value)
{
    if (m_properties.cornerRadius == value)
        return;

    m_properties.cornerRadius = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::CornerRadiusChanged);
}

void PlatformCALayerRemote::setEdgeAntialiasingMask(unsigned value)
{
    m_properties.edgeAntialiasingMask = value;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::EdgeAntialiasingMaskChanged);
}

FloatRoundedRect PlatformCALayerRemote::shapeRoundedRect() const
{
    return m_properties.shapeRoundedRect ? *m_properties.shapeRoundedRect : FloatRoundedRect(FloatRect());
}

void PlatformCALayerRemote::setShapeRoundedRect(const FloatRoundedRect& roundedRect)
{
    if (m_properties.shapeRoundedRect && *m_properties.shapeRoundedRect == roundedRect)
        return;

    m_properties.shapeRoundedRect = std::make_unique<FloatRoundedRect>(roundedRect);
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ShapeRoundedRectChanged);
}

Path PlatformCALayerRemote::shapePath() const
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    return m_properties.shapePath;
}

void PlatformCALayerRemote::setShapePath(const Path& path)
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    m_properties.shapePath = path;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::ShapePathChanged);
}

WindRule PlatformCALayerRemote::shapeWindRule() const
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    return m_properties.windRule;
}

void PlatformCALayerRemote::setShapeWindRule(WindRule windRule)
{
    ASSERT(m_layerType == LayerTypeShapeLayer);
    m_properties.windRule = windRule;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::WindRuleChanged);
}

bool PlatformCALayerRemote::requiresCustomAppearanceUpdateOnBoundsChange() const
{
    return m_properties.customAppearance == GraphicsLayer::CustomAppearance::ScrollingShadow;
}

GraphicsLayer::CustomAppearance PlatformCALayerRemote::customAppearance() const
{
    return m_properties.customAppearance;
}

void PlatformCALayerRemote::updateCustomAppearance(GraphicsLayer::CustomAppearance customAppearance)
{
    m_properties.customAppearance = customAppearance;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::CustomAppearanceChanged);
}

const Region* PlatformCALayerRemote::eventRegion() const
{
    return m_properties.eventRegion.get();
}

void PlatformCALayerRemote::setEventRegion(const WebCore::Region* eventRegion)
{
    const auto* oldEventRegion = m_properties.eventRegion.get();
    if (arePointingToEqualData(oldEventRegion, eventRegion))
        return;

    m_properties.eventRegion = eventRegion ? std::make_unique<WebCore::Region>(*eventRegion) : nullptr;
    m_properties.notePropertiesChanged(RemoteLayerTreeTransaction::EventRegionChanged);
}

GraphicsLayer::EmbeddedViewID PlatformCALayerRemote::embeddedViewID() const
{
    return m_embeddedViewID;
}

Ref<PlatformCALayer> PlatformCALayerRemote::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    return PlatformCALayerRemote::create(layerType, client, *m_context);
}

void PlatformCALayerRemote::enumerateRectsBeingDrawn(CGContextRef context, void (^block)(CGRect))
{
    m_properties.backingStore->enumerateRectsBeingDrawn(context, block);
}

uint32_t PlatformCALayerRemote::hostingContextID()
{
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned PlatformCALayerRemote::backingStoreBytesPerPixel() const
{
    if (!m_properties.backingStore)
        return 4;

    return m_properties.backingStore->bytesPerPixel();
}

LayerPool& PlatformCALayerRemote::layerPool()
{
    return m_context->layerPool();
}

} // namespace WebKit
