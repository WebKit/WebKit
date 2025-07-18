/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PlatformCALayerRemote.h"

#import "PlatformCALayerRemoteCustom.h"
#import "PlatformCALayerRemoteModelHosting.h"
#import "PlatformCALayerRemoteTiledBacking.h"
#import "RemoteLayerBackingStore.h"
#import "RemoteLayerTreeContext.h"
#import "RemoteLayerTreePropertyApplier.h"
#import <WebCore/AnimationUtilities.h>
#import <WebCore/ColorSpaceCG.h>
#import <WebCore/ContentsFormatCocoa.h>
#import <WebCore/EventRegion.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/IOSurface.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/PlatformCAFilters.h>
#import <WebCore/PlatformCALayerCocoa.h>
#import <WebCore/TiledBacking.h>
#import <wtf/PointerComparison.h>

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#import <WebCore/AcceleratedEffect.h>
#import <WebCore/AcceleratedEffectValues.h>
#endif

#if ENABLE(MODEL_PROCESS)
#import <WebCore/ModelContext.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(PlatformCALayer::LayerType layerType, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    RefPtr<PlatformCALayerRemote> layer;

    if (layerType == WebCore::PlatformCALayer::LayerType::LayerTypeTiledBackingLayer || layerType == WebCore::PlatformCALayer::LayerType::LayerTypePageTiledBackingLayer)
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

#if ENABLE(MODEL_PROCESS)
Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(Ref<WebCore::ModelContext> modelContext, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    return PlatformCALayerRemoteCustom::create(modelContext, owner, context);
}
#endif

#if ENABLE(MODEL_ELEMENT)
Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(Ref<WebCore::Model> model, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    return PlatformCALayerRemoteModelHosting::create(model, owner, context);
}
#endif

#if HAVE(AVKIT)
Ref<PlatformCALayerRemote> PlatformCALayerRemote::create(WebCore::HTMLVideoElement& videoElement, WebCore::PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
{
    return PlatformCALayerRemoteCustom::create(videoElement, owner, context);
}
#endif

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
    if (owner && layerType != PlatformCALayer::LayerType::LayerTypeContentsProvidedLayer && layerType != PlatformCALayer::LayerType::LayerTypeTransformLayer) {
        m_properties.contentsScale = owner->platformCALayerDeviceScaleFactor();
        m_properties.notePropertiesChanged(LayerChange::ContentsScaleChanged);
    }
}

PlatformCALayerRemote::PlatformCALayerRemote(const PlatformCALayerRemote& other, PlatformCALayerClient* owner, RemoteLayerTreeContext& context)
    : PlatformCALayer(other.layerType(), owner)
    , m_acceleratesDrawing(other.acceleratesDrawing())
    , m_context(&context)
{
}

Ref<PlatformCALayer> PlatformCALayerRemote::clone(PlatformCALayerClient* owner) const
{
    RELEASE_ASSERT(m_context.get());
    auto clone = PlatformCALayerRemote::create(*this, owner, *m_context);

    updateClonedLayerProperties(clone);

    clone->setClonedLayer(this);
    return WTFMove(clone);
}

PlatformCALayerRemote::~PlatformCALayerRemote()
{
    for (const auto& layer : m_children)
        downcast<PlatformCALayerRemote>(*layer).m_superlayer = nullptr;

    if (RefPtr<RemoteLayerTreeContext> protectedContext = m_context.get())
        protectedContext->layerWillLeaveContext(*this);
}

void PlatformCALayerRemote::moveToContext(RemoteLayerTreeContext& context)
{
    if (RefPtr protectedContext = m_context.get())
        protectedContext->layerWillLeaveContext(*this);

    m_context = context;

    context.layerDidEnterContext(*this, layerType());

    m_properties.notePropertiesChanged(m_properties.everChangedProperties);
}

void PlatformCALayerRemote::populateCreationProperties(RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeContext& context, PlatformCALayer::LayerType type)
{
    properties.layerID = layerID();
    properties.type = type;
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
    clone.setVideoGravity(videoGravity());
    clone.setBackdropRootIsOpaque(backdropRootIsOpaque());

    if (!m_properties.shapePath.isEmpty())
        clone.setShapePath(m_properties.shapePath);

    if (m_properties.shapeRoundedRect)
        clone.setShapeRoundedRect(*m_properties.shapeRoundedRect);

    if (m_properties.filters)
        clone.copyFiltersFrom(*this);

    clone.updateCustomAppearance(customAppearance());
}

void PlatformCALayerRemote::recursiveMarkWillBeDisplayedWithRenderingSuppresion()
{
    if (m_properties.backingStoreOrProperties.store && m_properties.backingStoreAttached)
        m_properties.backingStoreOrProperties.store->layerWillBeDisplayedWithRenderingSuppression();

    for (size_t i = 0; i < m_children.size(); ++i) {
        Ref child = downcast<PlatformCALayerRemote>(*m_children[i]);
        ASSERT(child->superlayer() == this);
        child->recursiveMarkWillBeDisplayedWithRenderingSuppresion();
    }
}

void PlatformCALayerRemote::recursiveBuildTransaction(RemoteLayerTreeContext& context, RemoteLayerTreeTransaction& transaction)
{
    ASSERT(!m_properties.backingStoreOrProperties.store || owner());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&context == m_context);

    if (owner() && owner()->platformCALayerRenderingIsSuppressedIncludingDescendants()) {
        // Rendering is suppressed, so don't include any mutations from this subtree
        // in the transaction. We do still mark all existing layers as will be displayed though,
        // to prevent the previous contents from being discarded.
        recursiveMarkWillBeDisplayedWithRenderingSuppresion();
        return;
    }

    bool usesBackingStore = owner() && (owner()->platformCALayerDrawsContent() || owner()->platformCALayerDelegatesDisplay(this));
    if (m_properties.backingStoreOrProperties.store && !usesBackingStore) {
        m_properties.backingStoreOrProperties.store = nullptr;
        m_properties.notePropertiesChanged(LayerChange::BackingStoreChanged);
    }

    if (m_properties.backingStoreOrProperties.store && m_properties.backingStoreAttached && m_properties.backingStoreOrProperties.store->layerWillBeDisplayed())
        m_properties.notePropertiesChanged(LayerChange::BackingStoreChanged);

    if (m_properties.changedProperties) {
        if (m_properties.changedProperties & LayerChange::ChildrenChanged) {
            m_properties.children = WTF::map(m_children, [](auto& child) {
                return child->layerID();
            });
        }

        // FIXME: the below is only necessary when blockMediaLayerRehostingInWebContentProcess() is disabled.
        // Once that setting is made unnecessary, remove this entire conditional as well.
        if (type() == PlatformCALayer::Type::RemoteCustom
            && !downcast<PlatformCALayerRemoteCustom>(*this).hasVideo()) {
            RemoteLayerTreePropertyApplier::applyPropertiesToLayer(platformLayer(), nullptr, nullptr, m_properties);
            didCommit();
            return;
        }

        transaction.layerPropertiesChanged(*this);
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        Ref child = downcast<PlatformCALayerRemote>(*m_children[i]);
        ASSERT(child->superlayer() == this);
        child->recursiveBuildTransaction(context, transaction);
    }

    if (m_maskLayer)
        downcast<PlatformCALayerRemote>(*m_maskLayer).recursiveBuildTransaction(context, transaction);
}

void PlatformCALayerRemote::didCommit()
{
    m_properties.animationChanges.addedAnimations.clear();
    m_properties.animationChanges.keysOfAnimationsToRemove.clear();
    m_properties.resetChangedProperties();
}

void PlatformCALayerRemote::ensureBackingStore()
{
    ASSERT(owner());
    ASSERT(m_properties.backingStoreAttached);

    bool needsNewBackingStore = [&] {
        if (!m_context)
            return false;

        if (!m_properties.backingStoreOrProperties.store)
            return true;

        // A layer pulled out of a pool may have existing backing store which we mustn't reuse if it lives in the wrong process.
        if (m_properties.backingStoreOrProperties.store->processModel() != RemoteLayerBackingStore::processModelForLayer(*this))
            return true;

        return false;
    }();

    if (needsNewBackingStore)
        m_properties.backingStoreOrProperties.store = RemoteLayerBackingStore::createForLayer(*this);

    updateBackingStore();
}

DestinationColorSpace PlatformCALayerRemote::displayColorSpace() const
{
#if PLATFORM(IOS_FAMILY)
    if (auto displayColorSpace = contentsFormatExtendedColorSpace(contentsFormat()))
        return displayColorSpace.value();
#else
    if (auto displayColorSpace = m_context ? m_context->displayColorSpace() : std::nullopt) {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        if (contentsFormat() == ContentsFormat::RGBA16F) {
            if (auto extendedDisplayColorSpace = displayColorSpace->asExtended())
                return extendedDisplayColorSpace.value();
        }
#endif
        return displayColorSpace.value();
    }
#endif

    return DestinationColorSpace::SRGB();
}

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
bool PlatformCALayerRemote::allowsDynamicContentScaling() const
{
    return owner() && owner()->platformCALayerAllowsDynamicContentScaling(this);
}

IncludeDynamicContentScalingDisplayList PlatformCALayerRemote::shouldIncludeDisplayListInBackingStore() const
{
    if (m_context && !m_context->useDynamicContentScalingDisplayListsForDOMRendering())
        return IncludeDynamicContentScalingDisplayList::No;
    if (!allowsDynamicContentScaling())
        return IncludeDynamicContentScalingDisplayList::No;
    return IncludeDynamicContentScalingDisplayList::Yes;
}
#endif

void PlatformCALayerRemote::updateBackingStore()
{
    if (!m_properties.backingStoreOrProperties.store)
        return;

    ASSERT(m_properties.backingStoreAttached);

    RemoteLayerBackingStore::Parameters parameters;
    parameters.type = m_acceleratesDrawing ? RemoteLayerBackingStore::Type::IOSurface : RemoteLayerBackingStore::Type::Bitmap;
    parameters.size = m_properties.bounds.size();

    parameters.colorSpace = displayColorSpace();
    parameters.contentsFormat = contentsFormat();
    parameters.scale = m_properties.contentsScale;
    parameters.isOpaque = m_properties.opaque;

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    parameters.includeDisplayList = shouldIncludeDisplayListInBackingStore();
#endif

    m_properties.backingStoreOrProperties.store->ensureBackingStore(parameters);
}

void PlatformCALayerRemote::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (!m_properties.backingStoreAttached)
        return;

    ensureBackingStore();

    // FIXME: Need to map this through contentsRect/etc.
    m_properties.backingStoreOrProperties.store->setNeedsDisplay(enclosingIntRect(rect));
}

void PlatformCALayerRemote::setNeedsDisplay()
{
    if (!m_properties.backingStoreAttached)
        return;

    ensureBackingStore();

    m_properties.backingStoreOrProperties.store->setNeedsDisplay();
}

bool PlatformCALayerRemote::needsDisplay() const
{
    if (!m_properties.backingStoreOrProperties.store)
        return false;

    return m_properties.backingStoreOrProperties.store->needsDisplay();
}

void PlatformCALayerRemote::copyContentsFromLayer(PlatformCALayer* layer)
{
    ASSERT(m_properties.clonedLayerID == layer->layerID());
    
    if (RefPtr protectedContext = m_context.get(); protectedContext && !m_properties.changedProperties)
        protectedContext->layerPropertyChangedWhileBuildingTransaction(*this);

    m_properties.notePropertiesChanged(LayerChange::ClonedContentsChanged);
}

PlatformCALayer* PlatformCALayerRemote::superlayer() const
{
    return m_superlayer.get();
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
        m_children.removeAt(childIndex);
    layer->m_superlayer = nullptr;
    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
}

void PlatformCALayerRemote::setSublayers(const PlatformCALayerList& list)
{
    removeAllSublayers();
    m_children = list;

    for (const auto& layer : list) {
        layer->removeFromSuperlayer();
        downcast<PlatformCALayerRemote>(*layer).m_superlayer = *this;
    }

    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
}

void PlatformCALayerRemote::removeAllSublayers()
{
    PlatformCALayerList layersToRemove = m_children;
    for (const auto& layer : layersToRemove)
        layer->removeFromSuperlayer();
    ASSERT(m_children.isEmpty());
    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
}

void PlatformCALayerRemote::appendSublayer(PlatformCALayer& layer)
{
    Ref<PlatformCALayer> protectedLayer(layer);

    layer.removeFromSuperlayer();
    m_children.append(&layer);
    downcast<PlatformCALayerRemote>(layer).m_superlayer = *this;
    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
}

void PlatformCALayerRemote::insertSublayer(PlatformCALayer& layer, size_t index)
{
    Ref<PlatformCALayer> protectedLayer(layer);

    layer.removeFromSuperlayer();
    m_children.insert(index, &layer);
    downcast<PlatformCALayerRemote>(layer).m_superlayer = *this;
    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
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
        downcast<PlatformCALayerRemote>(layer).m_superlayer = *this;
    }

    m_properties.notePropertiesChanged(LayerChange::ChildrenChanged);
}

void PlatformCALayerRemote::adoptSublayers(PlatformCALayer& source)
{
    PlatformCALayerList layersToMove = downcast<PlatformCALayerRemote>(source).m_children;

    if (const PlatformCALayerList* customLayers = source.customSublayers()) {
        for (const auto& layer : *customLayers) {
            size_t layerIndex = layersToMove.find(layer);
            if (layerIndex != notFound)
                layersToMove.removeAt(layerIndex);
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
        for (auto& keyAnimationPair : m_properties.animationChanges.addedAnimations) {
            if (keyAnimationPair.first == key) {
                keyAnimationPair.second = downcast<PlatformCAAnimationRemote>(animation).properties();
                appendToAddedAnimations = false;
                break;
            }
        }
    }
    if (appendToAddedAnimations)
        m_properties.animationChanges.addedAnimations.append(std::pair<String, PlatformCAAnimationRemote::Properties>(key, downcast<PlatformCAAnimationRemote>(animation).properties()));
    
    m_properties.notePropertiesChanged(LayerChange::AnimationsChanged);

    if (RefPtr protectedContext = m_context.get())
        protectedContext->willStartAnimationOnLayer(*this);
}

void PlatformCALayerRemote::removeAnimationForKey(const String& key)
{
    if (m_animations.remove(key)) {
        m_properties.animationChanges.addedAnimations.removeFirstMatching([&key](auto& pair) {
            return pair.first == key;
        });
    }
    m_properties.animationChanges.keysOfAnimationsToRemove.add(key);
    m_properties.notePropertiesChanged(LayerChange::AnimationsChanged);
}

RefPtr<PlatformCAAnimation> PlatformCALayerRemote::animationForKey(const String& key)
{
    return m_animations.get(key);
}

static inline bool isEquivalentLayer(const PlatformCALayer* layer, const std::optional<PlatformLayerIdentifier>& layerID)
{
    auto newLayerID = layer ? std::optional { layer->layerID() } : std::nullopt;
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

void PlatformCALayerRemote::setMaskLayer(RefPtr<WebCore::PlatformCALayer>&& layer)
{
    if (isEquivalentLayer(layer.get(), m_properties.maskLayerID))
        return;

    PlatformCALayer::setMaskLayer(WTFMove(layer));

    if (RefPtr layer = maskLayer())
        m_properties.maskLayerID = layer->layerID();
    else
        m_properties.maskLayerID = { };

    m_properties.notePropertiesChanged(LayerChange::MaskLayerChanged);
}

void PlatformCALayerRemote::setClonedLayer(const PlatformCALayer* layer)
{
    if (isEquivalentLayer(layer, m_properties.clonedLayerID))
        return;

    if (layer)
        m_properties.clonedLayerID = layer->layerID();
    else
        m_properties.clonedLayerID = { };

    m_properties.notePropertiesChanged(LayerChange::ClonedContentsChanged);
}

bool PlatformCALayerRemote::isOpaque() const
{
    return m_properties.opaque;
}

void PlatformCALayerRemote::setOpaque(bool value)
{
    m_properties.opaque = value;

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
    m_properties.notePropertiesChanged(LayerChange::BoundsChanged);
    
    if (requiresCustomAppearanceUpdateOnBoundsChange())
        m_properties.notePropertiesChanged(LayerChange::CustomAppearanceChanged);

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
    m_properties.notePropertiesChanged(LayerChange::PositionChanged);
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
    m_properties.notePropertiesChanged(LayerChange::AnchorPointChanged);
}

TransformationMatrix PlatformCALayerRemote::transform() const
{
    return m_properties.transform ? *m_properties.transform : TransformationMatrix();
}

void PlatformCALayerRemote::setTransform(const TransformationMatrix& value)
{
    m_properties.transform = makeUnique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(LayerChange::TransformChanged);
}

TransformationMatrix PlatformCALayerRemote::sublayerTransform() const
{
    return m_properties.sublayerTransform ? *m_properties.sublayerTransform : TransformationMatrix();
}

void PlatformCALayerRemote::setSublayerTransform(const TransformationMatrix& value)
{
    m_properties.sublayerTransform = makeUnique<TransformationMatrix>(value);
    m_properties.notePropertiesChanged(LayerChange::SublayerTransformChanged);
}

void PlatformCALayerRemote::setIsBackdropRoot(bool isBackdropRoot)
{
    m_properties.backdropRoot = isBackdropRoot;
    m_properties.notePropertiesChanged(LayerChange::BackdropRootChanged);
}

bool PlatformCALayerRemote::backdropRootIsOpaque() const
{
    return m_properties.backdropRootIsOpaque;
}

void PlatformCALayerRemote::setBackdropRootIsOpaque(bool backdropRootIsOpaque)
{
    m_properties.backdropRootIsOpaque = backdropRootIsOpaque;
    m_properties.notePropertiesChanged(LayerChange::BackdropRootIsOpaqueChanged);
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
    m_properties.notePropertiesChanged(LayerChange::HiddenChanged);
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
    m_properties.notePropertiesChanged(LayerChange::ContentsHiddenChanged);
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
    m_properties.notePropertiesChanged(LayerChange::UserInteractionEnabledChanged);
}

void PlatformCALayerRemote::setBackingStoreAttached(bool attached)
{
    if (m_properties.backingStoreAttached == attached)
        return;

    m_properties.backingStoreAttached = attached;
    m_properties.notePropertiesChanged(LayerChange::BackingStoreAttachmentChanged);
    
    if (attached)
        setNeedsDisplay();
    else
        m_properties.backingStoreOrProperties.store = nullptr;
}

bool PlatformCALayerRemote::backingStoreAttached() const
{
    return m_properties.backingStoreAttached;
}

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
void PlatformCALayerRemote::setVisibleRect(const FloatRect& value)
{
    m_properties.visibleRect = value;
    m_properties.notePropertiesChanged(LayerChange::VisibleRectChanged);
}
#endif

void PlatformCALayerRemote::setGeometryFlipped(bool value)
{
    m_properties.geometryFlipped = value;
    m_properties.notePropertiesChanged(LayerChange::GeometryFlippedChanged);
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
    m_properties.notePropertiesChanged(LayerChange::DoubleSidedChanged);
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
    m_properties.notePropertiesChanged(LayerChange::MasksToBoundsChanged);
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

ContentsFormat PlatformCALayerRemote::contentsFormat() const
{
    return m_properties.contentsFormat;
}

void PlatformCALayerRemote::setContentsFormat(ContentsFormat contentsFormat)
{
    if (m_properties.contentsFormat == contentsFormat)
        return;

    m_properties.contentsFormat = contentsFormat;
    updateBackingStore();
}

bool PlatformCALayerRemote::hasContents() const
{
    return !!m_properties.backingStoreOrProperties.store;
}

CFTypeRef PlatformCALayerRemote::contents() const
{
    return nullptr;
}

void PlatformCALayerRemote::setContents(CFTypeRef value)
{
    if (!m_properties.backingStoreOrProperties.store)
        return;
    if (!value)
        m_properties.backingStoreOrProperties.store->clearBackingStore();
}

void PlatformCALayerRemote::setDelegatedContents(const PlatformCALayerDelegatedContents& contents)
{
    setRemoteDelegatedContents({ ImageBufferBackendHandle { MachSendRight { contents.surface } }, contents.finishedFence, contents.surfaceIdentifier });
}

void PlatformCALayerRemote::setRemoteDelegatedContents(const PlatformCALayerRemoteDelegatedContents& contents)
{
    ASSERT(m_acceleratesDrawing);
    ensureBackingStore();
    m_properties.backingStoreOrProperties.store->setDelegatedContents(contents);
}

void PlatformCALayerRemote::setContentsRect(const FloatRect& value)
{
    m_properties.contentsRect = value;
    m_properties.notePropertiesChanged(LayerChange::ContentsRectChanged);
}

void PlatformCALayerRemote::setMinificationFilter(FilterType value)
{
    m_properties.minificationFilter = value;
    m_properties.notePropertiesChanged(LayerChange::MinificationFilterChanged);
}

void PlatformCALayerRemote::setMagnificationFilter(FilterType value)
{
    m_properties.magnificationFilter = value;
    m_properties.notePropertiesChanged(LayerChange::MagnificationFilterChanged);
}

Color PlatformCALayerRemote::backgroundColor() const
{
    return m_properties.backgroundColor;
}

void PlatformCALayerRemote::setBackgroundColor(const Color& value)
{
    if (value == m_properties.backgroundColor)
        return;

    m_properties.backgroundColor = value;
    m_properties.notePropertiesChanged(LayerChange::BackgroundColorChanged);
}

void PlatformCALayerRemote::setBorderWidth(float value)
{
    if (value == m_properties.borderWidth)
        return;

    m_properties.borderWidth = value;
    m_properties.notePropertiesChanged(LayerChange::BorderWidthChanged);
}

void PlatformCALayerRemote::setBorderColor(const Color& value)
{
    if (value == m_properties.borderColor)
        return;

    m_properties.borderColor = value;
    m_properties.notePropertiesChanged(LayerChange::BorderColorChanged);
}

float PlatformCALayerRemote::opacity() const
{
    return m_properties.opacity;
}

void PlatformCALayerRemote::setOpacity(float value)
{
    m_properties.opacity = value;
    m_properties.notePropertiesChanged(LayerChange::OpacityChanged);
}

void PlatformCALayerRemote::setFilters(const FilterOperations& filters)
{
    m_properties.filters = makeUnique<FilterOperations>(filters);
    m_properties.notePropertiesChanged(LayerChange::FiltersChanged);
}

void PlatformCALayerRemote::copyFiltersFrom(const PlatformCALayer& sourceLayer)
{
    if (const FilterOperations* filters = downcast<PlatformCALayerRemote>(sourceLayer).m_properties.filters.get())
        setFilters(*filters);
    else if (m_properties.filters)
        m_properties.filters = nullptr;

    m_properties.notePropertiesChanged(LayerChange::FiltersChanged);
}

void PlatformCALayerRemote::setBlendMode(BlendMode blendMode)
{
    m_properties.blendMode = blendMode;
    m_properties.notePropertiesChanged(LayerChange::BlendModeChanged);
}

bool PlatformCALayerRemote::filtersCanBeComposited(const FilterOperations& filters)
{
    return PlatformCALayerCocoa::filtersCanBeComposited(filters);
}

void PlatformCALayerRemote::setName(const String& value)
{
    m_properties.name = value;
    m_properties.notePropertiesChanged(LayerChange::NameChanged);
}

void PlatformCALayerRemote::setSpeed(float value)
{
    m_properties.speed = value;
    m_properties.notePropertiesChanged(LayerChange::SpeedChanged);
}

void PlatformCALayerRemote::setTimeOffset(CFTimeInterval value)
{
    m_properties.timeOffset = value;
    m_properties.notePropertiesChanged(LayerChange::TimeOffsetChanged);
}

float PlatformCALayerRemote::contentsScale() const
{
    return m_properties.contentsScale;
}

void PlatformCALayerRemote::setContentsScale(float value)
{
    if (m_layerType == PlatformCALayer::LayerType::LayerTypeTransformLayer)
        return;

    m_properties.contentsScale = value;
    m_properties.notePropertiesChanged(LayerChange::ContentsScaleChanged);

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
    m_properties.notePropertiesChanged(LayerChange::CornerRadiusChanged);
}

void PlatformCALayerRemote::setAntialiasesEdges(bool antialiases)
{
    if (antialiases == m_properties.antialiasesEdges)
        return;

    m_properties.antialiasesEdges = antialiases;
    m_properties.notePropertiesChanged(LayerChange::AntialiasesEdgesChanged);
}

MediaPlayerVideoGravity PlatformCALayerRemote::videoGravity() const
{
    return m_properties.videoGravity;
}

void PlatformCALayerRemote::setVideoGravity(MediaPlayerVideoGravity gravity)
{
    if (m_properties.videoGravity == gravity)
        return;

    m_properties.videoGravity = gravity;
    m_properties.notePropertiesChanged(LayerChange::VideoGravityChanged);
}

FloatRoundedRect PlatformCALayerRemote::shapeRoundedRect() const
{
    return m_properties.shapeRoundedRect ? *m_properties.shapeRoundedRect : FloatRoundedRect(FloatRect());
}

void PlatformCALayerRemote::setShapeRoundedRect(const FloatRoundedRect& roundedRect)
{
    if (m_properties.shapeRoundedRect && *m_properties.shapeRoundedRect == roundedRect)
        return;

    m_properties.shapeRoundedRect = makeUnique<FloatRoundedRect>(roundedRect);
    m_properties.notePropertiesChanged(LayerChange::ShapeRoundedRectChanged);
}

Path PlatformCALayerRemote::shapePath() const
{
    ASSERT(m_layerType == PlatformCALayer::LayerType::LayerTypeShapeLayer);
    return m_properties.shapePath;
}

void PlatformCALayerRemote::setShapePath(const Path& path)
{
    ASSERT(m_layerType == PlatformCALayer::LayerType::LayerTypeShapeLayer);
    m_properties.shapePath = path;
    m_properties.notePropertiesChanged(LayerChange::ShapePathChanged);
}

WindRule PlatformCALayerRemote::shapeWindRule() const
{
    ASSERT(m_layerType == PlatformCALayer::LayerType::LayerTypeShapeLayer);
    return m_properties.windRule;
}

void PlatformCALayerRemote::setShapeWindRule(WindRule windRule)
{
    ASSERT(m_layerType == PlatformCALayer::LayerType::LayerTypeShapeLayer);
    m_properties.windRule = windRule;
    m_properties.notePropertiesChanged(LayerChange::WindRuleChanged);
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
    if (customAppearance == m_properties.customAppearance)
        return;

    m_properties.customAppearance = customAppearance;
    m_properties.notePropertiesChanged(LayerChange::CustomAppearanceChanged);
}

void PlatformCALayerRemote::setEventRegion(const EventRegion& eventRegion)
{
    if (m_properties.eventRegion == eventRegion)
        return;

    m_properties.eventRegion = eventRegion;
    m_properties.notePropertiesChanged(LayerChange::EventRegionChanged);
}

#if ENABLE(SCROLLING_THREAD)
std::optional<ScrollingNodeID> PlatformCALayerRemote::scrollingNodeID() const
{
    return m_properties.scrollingNodeID;
}

void PlatformCALayerRemote::setScrollingNodeID(std::optional<ScrollingNodeID> nodeID)
{
    if (nodeID == m_properties.scrollingNodeID)
        return;

    m_properties.scrollingNodeID = nodeID;
    m_properties.notePropertiesChanged(LayerChange::ScrollingNodeIDChanged);
}
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
bool PlatformCALayerRemote::setNeedsDisplayIfEDRHeadroomExceeds(float headroom)
{
    if (m_properties.backingStoreOrProperties.store)
        return m_properties.backingStoreOrProperties.store->setNeedsDisplayIfEDRHeadroomExceeds(headroom);
    return false;
}

void PlatformCALayerRemote::setTonemappingEnabled(bool value)
{
    if (m_properties.tonemappingEnabled == value)
        return;

    m_properties.tonemappingEnabled = value;
    m_properties.notePropertiesChanged(LayerChange::TonemappingEnabledChanged);
}

bool PlatformCALayerRemote::tonemappingEnabled() const
{
    return m_properties.tonemappingEnabled;
}
#endif

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
bool PlatformCALayerRemote::isSeparated() const
{
    return m_properties.isSeparated;
}

void PlatformCALayerRemote::setIsSeparated(bool value)
{
    if (m_properties.isSeparated == value)
        return;

    m_properties.isSeparated = value;
    m_properties.notePropertiesChanged(LayerChange::SeparatedChanged);
}

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
bool PlatformCALayerRemote::isSeparatedPortal() const
{
    return m_properties.isSeparatedPortal;
}

void PlatformCALayerRemote::setIsSeparatedPortal(bool value)
{
    if (m_properties.isSeparatedPortal == value)
        return;

    m_properties.isSeparatedPortal = value;
    m_properties.notePropertiesChanged(LayerChange::SeparatedPortalChanged);
}

bool PlatformCALayerRemote::isDescendentOfSeparatedPortal() const
{
    return m_properties.isDescendentOfSeparatedPortal;
}

void PlatformCALayerRemote::setIsDescendentOfSeparatedPortal(bool value)
{
    if (m_properties.isDescendentOfSeparatedPortal == value)
        return;

    m_properties.isDescendentOfSeparatedPortal = value;
    m_properties.notePropertiesChanged(LayerChange::DescendentOfSeparatedPortalChanged);
}
#endif
#endif

#if HAVE(CORE_MATERIAL)

WebCore::AppleVisualEffectData PlatformCALayerRemote::appleVisualEffectData() const
{
    return m_properties.appleVisualEffectData;
}

void PlatformCALayerRemote::setAppleVisualEffectData(WebCore::AppleVisualEffectData effectData)
{
    if (m_properties.appleVisualEffectData == effectData)
        return;

    m_properties.appleVisualEffectData = effectData;
    m_properties.notePropertiesChanged(LayerChange::AppleVisualEffectChanged);
}

#endif

Ref<PlatformCALayer> PlatformCALayerRemote::createCompatibleLayer(PlatformCALayer::LayerType layerType, PlatformCALayerClient* client) const
{
    RELEASE_ASSERT(m_context.get());
    return PlatformCALayerRemote::create(layerType, client, *m_context);
}

void PlatformCALayerRemote::enumerateRectsBeingDrawn(WebCore::GraphicsContext& context, void (^block)(WebCore::FloatRect))
{
    m_properties.backingStoreOrProperties.store->enumerateRectsBeingDrawn(context, block);
}

uint32_t PlatformCALayerRemote::hostingContextID()
{
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned PlatformCALayerRemote::backingStoreBytesPerPixel() const
{
    if (!m_properties.backingStoreOrProperties.store)
        return 4;

    return m_properties.backingStoreOrProperties.store->bytesPerPixel();
}

LayerPool* PlatformCALayerRemote::layerPool()
{
    return m_context ? &m_context->layerPool() : nullptr;
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
void PlatformCALayerRemote::clearAcceleratedEffectsAndBaseValues()
{
    m_properties.animationChanges.effects = { };
    m_properties.animationChanges.baseValues = { };

    m_properties.notePropertiesChanged(LayerChange::AnimationsChanged);
}

void PlatformCALayerRemote::setAcceleratedEffectsAndBaseValues(const AcceleratedEffects& effects, const AcceleratedEffectValues& baseValues)
{
    m_properties.animationChanges.effects = effects;
    m_properties.animationChanges.baseValues = baseValues;

    m_properties.notePropertiesChanged(LayerChange::AnimationsChanged);
}
#endif

void PlatformCALayerRemote::purgeFrontBufferForTesting()
{
    if (m_properties.backingStoreOrProperties.store)
        return m_properties.backingStoreOrProperties.store->purgeFrontBufferForTesting();
}

void PlatformCALayerRemote::purgeBackBufferForTesting()
{
    if (m_properties.backingStoreOrProperties.store)
        return m_properties.backingStoreOrProperties.store->purgeBackBufferForTesting();
}

void PlatformCALayerRemote::markFrontBufferVolatileForTesting()
{
    if (m_properties.backingStoreOrProperties.store)
        m_properties.backingStoreOrProperties.store->markFrontBufferVolatileForTesting();
}

} // namespace WebKit

