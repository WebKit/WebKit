/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteLayerTreeTransaction.h"

#import "ArgumentCoders.h"
#import "PlatformCAAnimationRemote.h"
#import "PlatformCALayerRemote.h"
#import "WebCoreArgumentCoders.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/EventRegion.h>
#import <WebCore/LengthFunctions.h>
#import <WebCore/Model.h>
#import <WebCore/TimingFunction.h>
#import <wtf/text/CString.h>
#import <wtf/text/TextStream.h>

namespace WebKit {

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&) = default;
RemoteLayerTreeTransaction& RemoteLayerTreeTransaction::operator=(RemoteLayerTreeTransaction&&) = default;

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties()
    : type(WebCore::PlatformCALayer::LayerTypeLayer)
    , playerIdentifier(std::nullopt)
    , naturalSize(std::nullopt)
    , hostingContextID(0)
    , hostingDeviceScaleFactor(1)
    , preservesFlip(false)
{
}

void RemoteLayerTreeTransaction::LayerCreationProperties::encode(IPC::Encoder& encoder) const
{
    encoder << layerID;
    encoder << type;
    
    // PlatformCALayerRemoteCustom
    encoder << playerIdentifier;
    encoder << naturalSize;
    encoder << initialSize;
    encoder << hostingContextID;
    encoder << hostingDeviceScaleFactor;
    encoder << preservesFlip;

    // PlatformCALayerRemoteHost
    encoder << hostIdentifier;

#if ENABLE(MODEL_ELEMENT)
    // PlatformCALayerRemoteModelHosting
    encoder << !!model;
    if (model)
        encoder << *model;
#endif
}

auto RemoteLayerTreeTransaction::LayerCreationProperties::decode(IPC::Decoder& decoder) -> std::optional<LayerCreationProperties>
{
    LayerCreationProperties result;
    if (!decoder.decode(result.layerID))
        return std::nullopt;

    if (!decoder.decode(result.type))
        return std::nullopt;
    
    // PlatformCALayerRemoteCustom
    if (!decoder.decode(result.playerIdentifier))
        return std::nullopt;
    if (!decoder.decode(result.naturalSize))
        return std::nullopt;
    if (!decoder.decode(result.initialSize))
        return std::nullopt;

    if (!decoder.decode(result.hostingContextID))
        return std::nullopt;

    if (!decoder.decode(result.hostingDeviceScaleFactor))
        return std::nullopt;
    
    if (!decoder.decode(result.preservesFlip))
        return std::nullopt;

    // PlatformCALayerRemoteHost
    if (!decoder.decode(result.hostIdentifier))
        return std::nullopt;

#if ENABLE(MODEL_ELEMENT)
    // PlatformCALayerRemoteModelHosting
    bool hasModel;
    if (!decoder.decode(hasModel))
        return std::nullopt;
    if (hasModel) {
        auto model = WebCore::Model::decode(decoder);
        if (!model)
            return std::nullopt;
        result.model = model;
    }
#endif

    return WTFMove(result);
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties() = default;

RemoteLayerTreeTransaction::LayerProperties::LayerProperties(const LayerProperties& other)
    : changedProperties(other.changedProperties)
    , name(other.name)
    , children(other.children)
    , addedAnimations(other.addedAnimations)
    , keysOfAnimationsToRemove(other.keysOfAnimationsToRemove)
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    , effects(other.effects)
    , baseValues(other.baseValues)
#endif
    , position(other.position)
    , anchorPoint(other.anchorPoint)
    , bounds(other.bounds)
    , contentsRect(other.contentsRect)
    , shapePath(other.shapePath)
    , maskLayerID(other.maskLayerID)
    , clonedLayerID(other.clonedLayerID)
#if ENABLE(SCROLLING_THREAD)
    , scrollingNodeID(other.scrollingNodeID)
#endif
    , timeOffset(other.timeOffset)
    , speed(other.speed)
    , contentsScale(other.contentsScale)
    , cornerRadius(other.cornerRadius)
    , borderWidth(other.borderWidth)
    , opacity(other.opacity)
    , backgroundColor(other.backgroundColor)
    , borderColor(other.borderColor)
    , customAppearance(other.customAppearance)
    , minificationFilter(other.minificationFilter)
    , magnificationFilter(other.magnificationFilter)
    , blendMode(other.blendMode)
    , windRule(other.windRule)
    , videoGravity(other.videoGravity)
    , antialiasesEdges(other.antialiasesEdges)
    , hidden(other.hidden)
    , backingStoreAttached(other.backingStoreAttached)
    , geometryFlipped(other.geometryFlipped)
    , doubleSided(other.doubleSided)
    , masksToBounds(other.masksToBounds)
    , opaque(other.opaque)
    , contentsHidden(other.contentsHidden)
    , userInteractionEnabled(other.userInteractionEnabled)
    , eventRegion(other.eventRegion)
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    , isSeparated(other.isSeparated)
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    , isSeparatedPortal(other.isSeparatedPortal)
    , isDescendentOfSeparatedPortal(other.isDescendentOfSeparatedPortal)
#endif
#endif
{
    // FIXME: LayerProperties should reference backing store by ID, so that two layers can have the same backing store (for clones).
    // FIXME: LayerProperties shouldn't be copyable; PlatformCALayerRemote::clone should copy the relevant properties.

    if (other.transform)
        transform = makeUnique<WebCore::TransformationMatrix>(*other.transform);

    if (other.sublayerTransform)
        sublayerTransform = makeUnique<WebCore::TransformationMatrix>(*other.sublayerTransform);

    if (other.filters)
        filters = makeUnique<WebCore::FilterOperations>(*other.filters);
}

void RemoteLayerTreeTransaction::LayerProperties::encode(IPC::Encoder& encoder) const
{
    encoder << changedProperties;

    if (changedProperties & LayerChange::NameChanged)
        encoder << name;

    if (changedProperties & LayerChange::ChildrenChanged)
        encoder << children;

    if (changedProperties & LayerChange::AnimationsChanged) {
        encoder << addedAnimations;
        encoder << keysOfAnimationsToRemove;
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        encoder << effects;
        encoder << baseValues;
#endif
    }

    if (changedProperties & LayerChange::PositionChanged)
        encoder << position;

    if (changedProperties & LayerChange::BoundsChanged)
        encoder << bounds;

    if (changedProperties & LayerChange::BackgroundColorChanged)
        encoder << backgroundColor;

    if (changedProperties & LayerChange::AnchorPointChanged)
        encoder << anchorPoint;

    if (changedProperties & LayerChange::BorderWidthChanged)
        encoder << borderWidth;

    if (changedProperties & LayerChange::BorderColorChanged)
        encoder << borderColor;

    if (changedProperties & LayerChange::OpacityChanged)
        encoder << opacity;

    if (changedProperties & LayerChange::TransformChanged)
        encoder << *transform;

    if (changedProperties & LayerChange::SublayerTransformChanged)
        encoder << *sublayerTransform;

    if (changedProperties & LayerChange::AntialiasesEdgesChanged)
        encoder << antialiasesEdges;

    if (changedProperties & LayerChange::HiddenChanged)
        encoder << hidden;

    if (changedProperties & LayerChange::GeometryFlippedChanged)
        encoder << geometryFlipped;

    if (changedProperties & LayerChange::DoubleSidedChanged)
        encoder << doubleSided;

    if (changedProperties & LayerChange::MasksToBoundsChanged)
        encoder << masksToBounds;

    if (changedProperties & LayerChange::OpaqueChanged)
        encoder << opaque;

    if (changedProperties & LayerChange::ContentsHiddenChanged)
        encoder << contentsHidden;

    if (changedProperties & LayerChange::MaskLayerChanged)
        encoder << maskLayerID;

    if (changedProperties & LayerChange::ClonedContentsChanged)
        encoder << clonedLayerID;

#if ENABLE(SCROLLING_THREAD)
    if (changedProperties & LayerChange::ScrollingNodeIDChanged)
        encoder << scrollingNodeID;
#endif

    if (changedProperties & LayerChange::ContentsRectChanged)
        encoder << contentsRect;

    if (changedProperties & LayerChange::ContentsScaleChanged)
        encoder << contentsScale;

    if (changedProperties & LayerChange::CornerRadiusChanged)
        encoder << cornerRadius;

    if (changedProperties & LayerChange::ShapeRoundedRectChanged)
        encoder << *shapeRoundedRect;

    if (changedProperties & LayerChange::ShapePathChanged)
        encoder << shapePath;

    if (changedProperties & LayerChange::MinificationFilterChanged)
        encoder << minificationFilter;

    if (changedProperties & LayerChange::MagnificationFilterChanged)
        encoder << magnificationFilter;

    if (changedProperties & LayerChange::BlendModeChanged)
        encoder << blendMode;

    if (changedProperties & LayerChange::WindRuleChanged)
        encoder << windRule;

    if (changedProperties & LayerChange::SpeedChanged)
        encoder << speed;

    if (changedProperties & LayerChange::TimeOffsetChanged)
        encoder << timeOffset;

    if (changedProperties & LayerChange::BackingStoreChanged) {
        bool hasFrontBuffer = backingStore && backingStore->hasFrontBuffer();
        encoder << hasFrontBuffer;
        if (hasFrontBuffer)
            encoder << *backingStore;
    }

    if (changedProperties & LayerChange::BackingStoreAttachmentChanged)
        encoder << backingStoreAttached;

    if (changedProperties & LayerChange::FiltersChanged)
        encoder << *filters;

    if (changedProperties & LayerChange::CustomAppearanceChanged)
        encoder << customAppearance;

    if (changedProperties & LayerChange::UserInteractionEnabledChanged)
        encoder << userInteractionEnabled;

    if (changedProperties & LayerChange::EventRegionChanged)
        encoder << eventRegion;

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (changedProperties & LayerChange::SeparatedChanged)
        encoder << isSeparated;

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    if (changedProperties & LayerChange::SeparatedPortalChanged)
        encoder << isSeparatedPortal;

    if (changedProperties & LayerChange::DescendentOfSeparatedPortalChanged)
        encoder << isDescendentOfSeparatedPortal;
#endif
#endif

    if (changedProperties & LayerChange::VideoGravityChanged)
        encoder << videoGravity;
}

bool RemoteLayerTreeTransaction::LayerProperties::decode(IPC::Decoder& decoder, LayerProperties& result)
{
    if (!decoder.decode(result.changedProperties))
        return false;

    if (result.changedProperties & LayerChange::NameChanged) {
        if (!decoder.decode(result.name))
            return false;
    }

    if (result.changedProperties & LayerChange::ChildrenChanged) {
        if (!decoder.decode(result.children))
            return false;

        for (auto& layerID : result.children) {
            if (!layerID)
                return false;
        }
    }

    if (result.changedProperties & LayerChange::AnimationsChanged) {
        if (!decoder.decode(result.addedAnimations))
            return false;

        if (!decoder.decode(result.keysOfAnimationsToRemove))
            return false;
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        if (!decoder.decode(result.effects))
            return false;

        if (!decoder.decode(result.baseValues))
            return false;
#endif
    }

    if (result.changedProperties & LayerChange::PositionChanged) {
        if (!decoder.decode(result.position))
            return false;
    }

    if (result.changedProperties & LayerChange::BoundsChanged) {
        if (!decoder.decode(result.bounds))
            return false;
    }

    if (result.changedProperties & LayerChange::BackgroundColorChanged) {
        if (!decoder.decode(result.backgroundColor))
            return false;
    }

    if (result.changedProperties & LayerChange::AnchorPointChanged) {
        if (!decoder.decode(result.anchorPoint))
            return false;
    }

    if (result.changedProperties & LayerChange::BorderWidthChanged) {
        if (!decoder.decode(result.borderWidth))
            return false;
    }

    if (result.changedProperties & LayerChange::BorderColorChanged) {
        if (!decoder.decode(result.borderColor))
            return false;
    }

    if (result.changedProperties & LayerChange::OpacityChanged) {
        if (!decoder.decode(result.opacity))
            return false;
    }

    if (result.changedProperties & LayerChange::TransformChanged) {
        WebCore::TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;
        
        result.transform = makeUnique<WebCore::TransformationMatrix>(transform);
    }

    if (result.changedProperties & LayerChange::SublayerTransformChanged) {
        WebCore::TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;

        result.sublayerTransform = makeUnique<WebCore::TransformationMatrix>(transform);
    }

    if (result.changedProperties & LayerChange::AntialiasesEdgesChanged) {
        if (!decoder.decode(result.antialiasesEdges))
            return false;
    }

    if (result.changedProperties & LayerChange::HiddenChanged) {
        if (!decoder.decode(result.hidden))
            return false;
    }

    if (result.changedProperties & LayerChange::GeometryFlippedChanged) {
        if (!decoder.decode(result.geometryFlipped))
            return false;
    }

    if (result.changedProperties & LayerChange::DoubleSidedChanged) {
        if (!decoder.decode(result.doubleSided))
            return false;
    }

    if (result.changedProperties & LayerChange::MasksToBoundsChanged) {
        if (!decoder.decode(result.masksToBounds))
            return false;
    }

    if (result.changedProperties & LayerChange::OpaqueChanged) {
        if (!decoder.decode(result.opaque))
            return false;
    }

    if (result.changedProperties & LayerChange::ContentsHiddenChanged) {
        if (!decoder.decode(result.contentsHidden))
            return false;
    }

    if (result.changedProperties & LayerChange::MaskLayerChanged) {
        if (!decoder.decode(result.maskLayerID))
            return false;
    }

    if (result.changedProperties & LayerChange::ClonedContentsChanged) {
        if (!decoder.decode(result.clonedLayerID))
            return false;
    }

#if ENABLE(SCROLLING_THREAD)
    if (result.changedProperties & LayerChange::ScrollingNodeIDChanged) {
        if (!decoder.decode(result.scrollingNodeID))
            return false;
    }
#endif

    if (result.changedProperties & LayerChange::ContentsRectChanged) {
        if (!decoder.decode(result.contentsRect))
            return false;
    }

    if (result.changedProperties & LayerChange::ContentsScaleChanged) {
        if (!decoder.decode(result.contentsScale))
            return false;
    }

    if (result.changedProperties & LayerChange::CornerRadiusChanged) {
        if (!decoder.decode(result.cornerRadius))
            return false;
    }

    if (result.changedProperties & LayerChange::ShapeRoundedRectChanged) {
        WebCore::FloatRoundedRect roundedRect;
        if (!decoder.decode(roundedRect))
            return false;
        
        result.shapeRoundedRect = makeUnique<WebCore::FloatRoundedRect>(roundedRect);
    }

    if (result.changedProperties & LayerChange::ShapePathChanged) {
        WebCore::Path path;
        if (!decoder.decode(path))
            return false;
        
        result.shapePath = WTFMove(path);
    }

    if (result.changedProperties & LayerChange::MinificationFilterChanged) {
        if (!decoder.decode(result.minificationFilter))
            return false;
    }

    if (result.changedProperties & LayerChange::MagnificationFilterChanged) {
        if (!decoder.decode(result.magnificationFilter))
            return false;
    }

    if (result.changedProperties & LayerChange::BlendModeChanged) {
        if (!decoder.decode(result.blendMode))
            return false;
    }

    if (result.changedProperties & LayerChange::WindRuleChanged) {
        if (!decoder.decode(result.windRule))
            return false;
    }

    if (result.changedProperties & LayerChange::SpeedChanged) {
        if (!decoder.decode(result.speed))
            return false;
    }

    if (result.changedProperties & LayerChange::TimeOffsetChanged) {
        if (!decoder.decode(result.timeOffset))
            return false;
    }

    if (result.changedProperties & LayerChange::BackingStoreChanged) {
        bool hasFrontBuffer = false;
        if (!decoder.decode(hasFrontBuffer))
            return false;
        if (hasFrontBuffer) {
            auto backingStore = makeUnique<RemoteLayerBackingStoreProperties>();
            if (!decoder.decode(*backingStore))
                return false;
            
            result.backingStoreProperties = WTFMove(backingStore);
        } else
            result.backingStoreProperties = nullptr;
    }

    if (result.changedProperties & LayerChange::BackingStoreAttachmentChanged) {
        if (!decoder.decode(result.backingStoreAttached))
            return false;
    }

    if (result.changedProperties & LayerChange::FiltersChanged) {
        auto filters = makeUnique<WebCore::FilterOperations>();
        if (!decoder.decode(*filters))
            return false;
        result.filters = WTFMove(filters);
    }

    if (result.changedProperties & LayerChange::CustomAppearanceChanged) {
        if (!decoder.decode(result.customAppearance))
            return false;
    }

    if (result.changedProperties & LayerChange::UserInteractionEnabledChanged) {
        if (!decoder.decode(result.userInteractionEnabled))
            return false;
    }

    if (result.changedProperties & LayerChange::EventRegionChanged) {
        std::optional<WebCore::EventRegion> eventRegion;
        decoder >> eventRegion;
        if (!eventRegion)
            return false;
        result.eventRegion = WTFMove(*eventRegion);
    }

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (result.changedProperties & LayerChange::SeparatedChanged) {
        if (!decoder.decode(result.isSeparated))
            return false;
    }

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    if (result.changedProperties & LayerChange::SeparatedPortalChanged) {
        if (!decoder.decode(result.isSeparatedPortal))
            return false;
    }

    if (result.changedProperties & LayerChange::DescendentOfSeparatedPortalChanged) {
        if (!decoder.decode(result.isDescendentOfSeparatedPortal))
            return false;
    }
#endif
#endif

    if (result.changedProperties & LayerChange::VideoGravityChanged) {
        if (!decoder.decode(result.videoGravity))
            return false;
    }

    return true;
}

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction() = default;
RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction() = default;

void RemoteLayerTreeTransaction::encode(IPC::Encoder& encoder) const
{
    encoder << m_rootLayerID;
    encoder << m_createdLayers;
    encoder << m_remoteContextHostedIdentifier;

    encoder << static_cast<uint64_t>(m_changedLayers.size());

    for (const auto& layer : m_changedLayers) {
        encoder << layer->layerID();
        encoder << layer->properties();
    }
    
    encoder << m_destroyedLayerIDs;
    encoder << m_videoLayerIDsPendingFullscreen;
    encoder << m_layerIDsWithNewlyUnreachableBackingStore;

    encoder << m_contentsSize;
    encoder << m_scrollOrigin;

    encoder << m_baseLayoutViewportSize;
    encoder << m_minStableLayoutViewportOrigin;
    encoder << m_maxStableLayoutViewportOrigin;

    encoder << m_scrollPosition;

    encoder << m_themeColor;
    encoder << m_pageExtendedBackgroundColor;
    encoder << m_sampledPageTopColor;

#if PLATFORM(MAC)
    encoder << m_pageScalingLayerID;
    encoder << m_scrolledContentsLayerID;
#endif

    encoder << m_pageScaleFactor;
    encoder << m_minimumScaleFactor;
    encoder << m_maximumScaleFactor;
    encoder << m_initialScaleFactor;
    encoder << m_viewportMetaTagWidth;

    encoder << m_renderTreeSize;
    encoder << m_transactionID;
    encoder << m_activityStateChangeID;

    encoder << m_newlyReachedPaintingMilestones;

    encoder << m_scaleWasSetByUIProcess;
    encoder << m_allowsUserScaling;

    encoder << m_avoidsUnsafeArea;

    encoder << m_viewportMetaTagWidthWasExplicit;
    encoder << m_viewportMetaTagCameFromImageDocument;

    encoder << m_isInStableState;

    encoder << m_callbackIDs;

    encoder << hasEditorState();
    if (m_editorState)
        encoder << *m_editorState;

#if PLATFORM(IOS_FAMILY)
    encoder << m_dynamicViewportSizeUpdateID;
#endif
}

bool RemoteLayerTreeTransaction::decode(IPC::Decoder& decoder, RemoteLayerTreeTransaction& result)
{
    if (!decoder.decode(result.m_rootLayerID))
        return false;
    if (!result.m_rootLayerID)
        return false;

    if (!decoder.decode(result.m_createdLayers))
        return false;

    if (!decoder.decode(result.m_remoteContextHostedIdentifier))
        return false;

    uint64_t numChangedLayerProperties;
    if (!decoder.decode(numChangedLayerProperties))
        return false;

    for (uint64_t i = 0; i < numChangedLayerProperties; ++i) {
        WebCore::PlatformLayerIdentifier layerID;
        if (!decoder.decode(layerID))
            return false;

        std::unique_ptr<LayerProperties> layerProperties = makeUnique<LayerProperties>();
        if (!decoder.decode(*layerProperties))
            return false;

        result.changedLayerProperties().set(layerID, WTFMove(layerProperties));
    }

    if (!decoder.decode(result.m_destroyedLayerIDs))
        return false;

    for (auto& layerID : result.m_destroyedLayerIDs) {
        if (!layerID)
            return false;
    }

    if (!decoder.decode(result.m_videoLayerIDsPendingFullscreen))
        return false;

    if (!decoder.decode(result.m_layerIDsWithNewlyUnreachableBackingStore))
        return false;

    for (auto& layerID : result.m_layerIDsWithNewlyUnreachableBackingStore) {
        if (!layerID)
            return false;
    }

    if (!decoder.decode(result.m_contentsSize))
        return false;

    if (!decoder.decode(result.m_scrollOrigin))
        return false;

    if (!decoder.decode(result.m_baseLayoutViewportSize))
        return false;

    if (!decoder.decode(result.m_minStableLayoutViewportOrigin))
        return false;

    if (!decoder.decode(result.m_maxStableLayoutViewportOrigin))
        return false;

    if (!decoder.decode(result.m_scrollPosition))
        return false;

    if (!decoder.decode(result.m_themeColor))
        return false;

    if (!decoder.decode(result.m_pageExtendedBackgroundColor))
        return false;

    if (!decoder.decode(result.m_sampledPageTopColor))
        return false;

#if PLATFORM(MAC)
    if (!decoder.decode(result.m_pageScalingLayerID))
        return false;
    if (!decoder.decode(result.m_scrolledContentsLayerID))
        return false;
#endif

    if (!decoder.decode(result.m_pageScaleFactor))
        return false;

    if (!decoder.decode(result.m_minimumScaleFactor))
        return false;

    if (!decoder.decode(result.m_maximumScaleFactor))
        return false;

    if (!decoder.decode(result.m_initialScaleFactor))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagWidth))
        return false;

    if (!decoder.decode(result.m_renderTreeSize))
        return false;

    if (!decoder.decode(result.m_transactionID))
        return false;

    if (!decoder.decode(result.m_activityStateChangeID))
        return false;

    if (!decoder.decode(result.m_newlyReachedPaintingMilestones))
        return false;

    if (!decoder.decode(result.m_scaleWasSetByUIProcess))
        return false;

    if (!decoder.decode(result.m_allowsUserScaling))
        return false;

    if (!decoder.decode(result.m_avoidsUnsafeArea))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagWidthWasExplicit))
        return false;

    if (!decoder.decode(result.m_viewportMetaTagCameFromImageDocument))
        return false;

    if (!decoder.decode(result.m_isInStableState))
        return false;

    std::optional<Vector<TransactionCallbackID>> callbackIDs;
    decoder >> callbackIDs;
    if (!callbackIDs)
        return false;
    result.m_callbackIDs = WTFMove(*callbackIDs);

    bool hasEditorState;
    if (!decoder.decode(hasEditorState))
        return false;

    if (hasEditorState) {
        EditorState editorState;
        if (!decoder.decode(editorState))
            return false;
        result.setEditorState(editorState);
    }

#if PLATFORM(IOS_FAMILY)
    if (!decoder.decode(result.m_dynamicViewportSizeUpdateID))
        return false;
#endif

    return true;
}

void RemoteLayerTreeTransaction::setRootLayerID(WebCore::PlatformLayerIdentifier rootLayerID)
{
    ASSERT_ARG(rootLayerID, rootLayerID);

    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote& remoteLayer)
{
    m_changedLayers.add(&remoteLayer);
}

void RemoteLayerTreeTransaction::setCreatedLayers(Vector<LayerCreationProperties> createdLayers)
{
    m_createdLayers = WTFMove(createdLayers);
}

void RemoteLayerTreeTransaction::setDestroyedLayerIDs(Vector<WebCore::PlatformLayerIdentifier> destroyedLayerIDs)
{
    m_destroyedLayerIDs = WTFMove(destroyedLayerIDs);
}

void RemoteLayerTreeTransaction::setLayerIDsWithNewlyUnreachableBackingStore(Vector<WebCore::PlatformLayerIdentifier> layerIDsWithNewlyUnreachableBackingStore)
{
    m_layerIDsWithNewlyUnreachableBackingStore = WTFMove(layerIDsWithNewlyUnreachableBackingStore);
}

#if !defined(NDEBUG) || !LOG_DISABLED

static void dumpChangedLayers(TextStream& ts, const RemoteLayerTreeTransaction::LayerPropertiesMap& changedLayerProperties)
{
    if (changedLayerProperties.isEmpty())
        return;

    TextStream::GroupScope group(ts);
    ts << "changed-layers";

    // Dump the layer properties sorted by layer ID.
    auto layerIDs = copyToVector(changedLayerProperties.keys());
    std::sort(layerIDs.begin(), layerIDs.end());

    for (auto& layerID : layerIDs) {
        const RemoteLayerTreeTransaction::LayerProperties& layerProperties = *changedLayerProperties.get(layerID);

        TextStream::GroupScope group(ts);
        ts << "layer " << layerID;

        if (layerProperties.changedProperties & LayerChange::NameChanged)
            ts.dumpProperty("name", layerProperties.name);

        if (layerProperties.changedProperties & LayerChange::ChildrenChanged)
            ts.dumpProperty<Vector<WebCore::PlatformLayerIdentifier>>("children", layerProperties.children);

        if (layerProperties.changedProperties & LayerChange::PositionChanged)
            ts.dumpProperty("position", layerProperties.position);

        if (layerProperties.changedProperties & LayerChange::BoundsChanged)
            ts.dumpProperty("bounds", layerProperties.bounds);

        if (layerProperties.changedProperties & LayerChange::AnchorPointChanged)
            ts.dumpProperty("anchorPoint", layerProperties.anchorPoint);

        if (layerProperties.changedProperties & LayerChange::BackgroundColorChanged)
            ts.dumpProperty("backgroundColor", layerProperties.backgroundColor);

        if (layerProperties.changedProperties & LayerChange::BorderColorChanged)
            ts.dumpProperty("borderColor", layerProperties.borderColor);

        if (layerProperties.changedProperties & LayerChange::BorderWidthChanged)
            ts.dumpProperty("borderWidth", layerProperties.borderWidth);

        if (layerProperties.changedProperties & LayerChange::OpacityChanged)
            ts.dumpProperty("opacity", layerProperties.opacity);

        if (layerProperties.changedProperties & LayerChange::TransformChanged)
            ts.dumpProperty("transform", layerProperties.transform ? *layerProperties.transform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & LayerChange::SublayerTransformChanged)
            ts.dumpProperty("sublayerTransform", layerProperties.sublayerTransform ? *layerProperties.sublayerTransform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & LayerChange::HiddenChanged)
            ts.dumpProperty("hidden", layerProperties.hidden);

        if (layerProperties.changedProperties & LayerChange::GeometryFlippedChanged)
            ts.dumpProperty("geometryFlipped", layerProperties.geometryFlipped);

        if (layerProperties.changedProperties & LayerChange::DoubleSidedChanged)
            ts.dumpProperty("doubleSided", layerProperties.doubleSided);

        if (layerProperties.changedProperties & LayerChange::MasksToBoundsChanged)
            ts.dumpProperty("masksToBounds", layerProperties.masksToBounds);

        if (layerProperties.changedProperties & LayerChange::OpaqueChanged)
            ts.dumpProperty("opaque", layerProperties.opaque);

        if (layerProperties.changedProperties & LayerChange::ContentsHiddenChanged)
            ts.dumpProperty("contentsHidden", layerProperties.contentsHidden);

        if (layerProperties.changedProperties & LayerChange::MaskLayerChanged)
            ts.dumpProperty("maskLayer", layerProperties.maskLayerID);

        if (layerProperties.changedProperties & LayerChange::ClonedContentsChanged)
            ts.dumpProperty("clonedLayer", layerProperties.clonedLayerID);

#if ENABLE(SCROLLING_THREAD)
        if (layerProperties.changedProperties & LayerChange::ScrollingNodeIDChanged)
            ts.dumpProperty("scrollingNodeID", layerProperties.scrollingNodeID);
#endif

        if (layerProperties.changedProperties & LayerChange::ContentsRectChanged)
            ts.dumpProperty("contentsRect", layerProperties.contentsRect);

        if (layerProperties.changedProperties & LayerChange::ContentsScaleChanged)
            ts.dumpProperty("contentsScale", layerProperties.contentsScale);

        if (layerProperties.changedProperties & LayerChange::CornerRadiusChanged)
            ts.dumpProperty("cornerRadius", layerProperties.cornerRadius);

        if (layerProperties.changedProperties & LayerChange::ShapeRoundedRectChanged)
            ts.dumpProperty("shapeRect", layerProperties.shapeRoundedRect ? *layerProperties.shapeRoundedRect : WebCore::FloatRoundedRect());

        if (layerProperties.changedProperties & LayerChange::MinificationFilterChanged)
            ts.dumpProperty("minificationFilter", layerProperties.minificationFilter);

        if (layerProperties.changedProperties & LayerChange::MagnificationFilterChanged)
            ts.dumpProperty("magnificationFilter", layerProperties.magnificationFilter);

        if (layerProperties.changedProperties & LayerChange::BlendModeChanged)
            ts.dumpProperty("blendMode", layerProperties.blendMode);

        if (layerProperties.changedProperties & LayerChange::SpeedChanged)
            ts.dumpProperty("speed", layerProperties.speed);

        if (layerProperties.changedProperties & LayerChange::TimeOffsetChanged)
            ts.dumpProperty("timeOffset", layerProperties.timeOffset);

        if (layerProperties.changedProperties & LayerChange::BackingStoreChanged) {
            if (auto* backingStoreProperties = layerProperties.backingStoreProperties.get())
                ts.dumpProperty("backingStore", *backingStoreProperties);
            else
                ts.dumpProperty("backingStore", "removed");
        }

        if (layerProperties.changedProperties & LayerChange::BackingStoreAttachmentChanged)
            ts.dumpProperty("backingStoreAttached", layerProperties.backingStoreAttached);

        if (layerProperties.changedProperties & LayerChange::FiltersChanged)
            ts.dumpProperty("filters", layerProperties.filters ? *layerProperties.filters : WebCore::FilterOperations());

        if (layerProperties.changedProperties & LayerChange::AnimationsChanged) {
            for (const auto& keyAnimationPair : layerProperties.addedAnimations)
                ts.dumpProperty("animation " +  keyAnimationPair.first, keyAnimationPair.second);

            for (const auto& name : layerProperties.keysOfAnimationsToRemove)
                ts.dumpProperty("removed animation", name);
        }

        if (layerProperties.changedProperties & LayerChange::AntialiasesEdgesChanged)
            ts.dumpProperty("antialiasesEdges", layerProperties.antialiasesEdges);

        if (layerProperties.changedProperties & LayerChange::CustomAppearanceChanged)
            ts.dumpProperty("customAppearance", layerProperties.customAppearance);

        if (layerProperties.changedProperties & LayerChange::UserInteractionEnabledChanged)
            ts.dumpProperty("userInteractionEnabled", layerProperties.userInteractionEnabled);

        if (layerProperties.changedProperties & LayerChange::EventRegionChanged)
            ts.dumpProperty("eventRegion", layerProperties.eventRegion);

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        if (layerProperties.changedProperties & LayerChange::SeparatedChanged)
            ts.dumpProperty("isSeparated", layerProperties.isSeparated);

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
        if (layerProperties.changedProperties & LayerChange::SeparatedPortalChanged)
            ts.dumpProperty("isSeparatedPortal", layerProperties.isSeparatedPortal);

        if (layerProperties.changedProperties & LayerChange::DescendentOfSeparatedPortalChanged)
            ts.dumpProperty("isDescendentOfSeparatedPortal", layerProperties.isDescendentOfSeparatedPortal);
#endif
#endif

        if (layerProperties.changedProperties & LayerChange::VideoGravityChanged)
            ts.dumpProperty("videoGravity", layerProperties.videoGravity);
    }
}

void RemoteLayerTreeTransaction::dump() const
{
    WTFLogAlways("%s", description().utf8().data());
}

String RemoteLayerTreeTransaction::description() const
{
    TextStream ts;

    ts.startGroup();
    ts << "layer tree";

    ts.dumpProperty("transactionID", m_transactionID);
    ts.dumpProperty("contentsSize", m_contentsSize);
    if (m_scrollOrigin != WebCore::IntPoint::zero())
        ts.dumpProperty("scrollOrigin", m_scrollOrigin);

    ts.dumpProperty("baseLayoutViewportSize", WebCore::FloatSize(m_baseLayoutViewportSize));

    if (m_minStableLayoutViewportOrigin != WebCore::LayoutPoint::zero())
        ts.dumpProperty("minStableLayoutViewportOrigin", WebCore::FloatPoint(m_minStableLayoutViewportOrigin));
    ts.dumpProperty("maxStableLayoutViewportOrigin", WebCore::FloatPoint(m_maxStableLayoutViewportOrigin));

    if (m_pageScaleFactor != 1)
        ts.dumpProperty("pageScaleFactor", m_pageScaleFactor);

#if PLATFORM(MAC)
    ts.dumpProperty("pageScalingLayer", m_pageScalingLayerID);
    ts.dumpProperty("scrolledContentsLayerID", m_scrolledContentsLayerID);
#endif

    ts.dumpProperty("minimumScaleFactor", m_minimumScaleFactor);
    ts.dumpProperty("maximumScaleFactor", m_maximumScaleFactor);
    ts.dumpProperty("initialScaleFactor", m_initialScaleFactor);
    ts.dumpProperty("viewportMetaTagWidth", m_viewportMetaTagWidth);
    ts.dumpProperty("viewportMetaTagWidthWasExplicit", m_viewportMetaTagWidthWasExplicit);
    ts.dumpProperty("viewportMetaTagCameFromImageDocument", m_viewportMetaTagCameFromImageDocument);
    ts.dumpProperty("allowsUserScaling", m_allowsUserScaling);
    ts.dumpProperty("avoidsUnsafeArea", m_avoidsUnsafeArea);
    ts.dumpProperty("isInStableState", m_isInStableState);
    ts.dumpProperty("renderTreeSize", m_renderTreeSize);
    ts.dumpProperty("root-layer", m_rootLayerID);

    if (!m_createdLayers.isEmpty()) {
        TextStream::GroupScope group(ts);
        ts << "created-layers";
        for (const auto& createdLayer : m_createdLayers) {
            TextStream::GroupScope group(ts);
            ts << createdLayer.type <<" " << createdLayer.layerID;
            switch (createdLayer.type) {
            case WebCore::PlatformCALayer::LayerTypeAVPlayerLayer:
                ts << " (context-id " << createdLayer.hostingContextID << ")";
                break;
            case WebCore::PlatformCALayer::LayerTypeCustom:
                ts << " (context-id " << createdLayer.hostingContextID << ")";
                break;
#if ENABLE(MODEL_ELEMENT)
            case WebCore::PlatformCALayer::LayerTypeModelLayer:
                ts << " (model " << *createdLayer.model << ")";
                break;
#endif
            default:
                break;
            }
        }
    }

    dumpChangedLayers(ts, m_changedLayerProperties);

    if (!m_destroyedLayerIDs.isEmpty())
        ts.dumpProperty<Vector<WebCore::PlatformLayerIdentifier>>("destroyed-layers", m_destroyedLayerIDs);

    if (m_editorState) {
        TextStream::GroupScope scope(ts);
        ts << "EditorState";
        ts << *m_editorState;
    }

    ts.endGroup();

    return ts.release();
}

#endif // !defined(NDEBUG) || !LOG_DISABLED

} // namespace WebKit
