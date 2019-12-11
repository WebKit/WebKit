/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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
#import <WebCore/TimingFunction.h>
#import <wtf/text/CString.h>
#import <wtf/text/TextStream.h>

namespace WebKit {

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&) = default;
RemoteLayerTreeTransaction& RemoteLayerTreeTransaction::operator=(RemoteLayerTreeTransaction&&) = default;

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties()
    : layerID(0)
    , type(WebCore::PlatformCALayer::LayerTypeLayer)
    , embeddedViewID(0)
    , hostingContextID(0)
    , hostingDeviceScaleFactor(1)
{
}

void RemoteLayerTreeTransaction::LayerCreationProperties::encode(IPC::Encoder& encoder) const
{
    encoder << layerID;
    encoder.encodeEnum(type);
    encoder << embeddedViewID;
    encoder << hostingContextID;
    encoder << hostingDeviceScaleFactor;
}

auto RemoteLayerTreeTransaction::LayerCreationProperties::decode(IPC::Decoder& decoder) -> Optional<LayerCreationProperties>
{
    LayerCreationProperties result;
    if (!decoder.decode(result.layerID))
        return WTF::nullopt;

    if (!decoder.decodeEnum(result.type))
        return WTF::nullopt;

    if (!decoder.decode(result.embeddedViewID))
        return WTF::nullopt;

    if (!decoder.decode(result.hostingContextID))
        return WTF::nullopt;

    if (!decoder.decode(result.hostingDeviceScaleFactor))
        return WTF::nullopt;

    return WTFMove(result);
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties()
    : anchorPoint(0.5, 0.5, 0)
    , contentsRect(WebCore::FloatPoint(), WebCore::FloatSize(1, 1))
    , maskLayerID(0)
    , clonedLayerID(0)
    , timeOffset(0)
    , speed(1)
    , contentsScale(1)
    , cornerRadius(0)
    , borderWidth(0)
    , opacity(1)
    , backgroundColor(WebCore::Color::transparent)
    , borderColor(WebCore::Color::black)
    , edgeAntialiasingMask(kCALayerLeftEdge | kCALayerRightEdge | kCALayerBottomEdge | kCALayerTopEdge)
    , customAppearance(WebCore::GraphicsLayer::CustomAppearance::None)
    , minificationFilter(WebCore::PlatformCALayer::FilterType::Linear)
    , magnificationFilter(WebCore::PlatformCALayer::FilterType::Linear)
    , blendMode(WebCore::BlendMode::Normal)
    , windRule(WebCore::WindRule::NonZero)
    , hidden(false)
    , backingStoreAttached(true)
    , geometryFlipped(false)
    , doubleSided(true)
    , masksToBounds(false)
    , opaque(false)
    , contentsHidden(false)
    , userInteractionEnabled(true)
{
}

RemoteLayerTreeTransaction::LayerProperties::LayerProperties(const LayerProperties& other)
    : changedProperties(other.changedProperties)
    , name(other.name)
    , children(other.children)
    , addedAnimations(other.addedAnimations)
    , keyPathsOfAnimationsToRemove(other.keyPathsOfAnimationsToRemove)
    , position(other.position)
    , anchorPoint(other.anchorPoint)
    , bounds(other.bounds)
    , contentsRect(other.contentsRect)
    , shapePath(other.shapePath)
    , maskLayerID(other.maskLayerID)
    , clonedLayerID(other.clonedLayerID)
    , timeOffset(other.timeOffset)
    , speed(other.speed)
    , contentsScale(other.contentsScale)
    , cornerRadius(other.cornerRadius)
    , borderWidth(other.borderWidth)
    , opacity(other.opacity)
    , backgroundColor(other.backgroundColor)
    , borderColor(other.borderColor)
    , edgeAntialiasingMask(other.edgeAntialiasingMask)
    , customAppearance(other.customAppearance)
    , minificationFilter(other.minificationFilter)
    , magnificationFilter(other.magnificationFilter)
    , blendMode(other.blendMode)
    , windRule(other.windRule)
    , hidden(other.hidden)
    , backingStoreAttached(other.backingStoreAttached)
    , geometryFlipped(other.geometryFlipped)
    , doubleSided(other.doubleSided)
    , masksToBounds(other.masksToBounds)
    , opaque(other.opaque)
    , contentsHidden(other.contentsHidden)
    , userInteractionEnabled(other.userInteractionEnabled)
    , eventRegion(other.eventRegion)
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
    encoder.encode(changedProperties);

    if (changedProperties & NameChanged)
        encoder << name;

    if (changedProperties & ChildrenChanged)
        encoder << children;

    if (changedProperties & AnimationsChanged) {
        encoder << addedAnimations;
        encoder << keyPathsOfAnimationsToRemove;
    }

    if (changedProperties & PositionChanged)
        encoder << position;

    if (changedProperties & BoundsChanged)
        encoder << bounds;

    if (changedProperties & BackgroundColorChanged)
        encoder << backgroundColor;

    if (changedProperties & AnchorPointChanged)
        encoder << anchorPoint;

    if (changedProperties & BorderWidthChanged)
        encoder << borderWidth;

    if (changedProperties & BorderColorChanged)
        encoder << borderColor;

    if (changedProperties & OpacityChanged)
        encoder << opacity;

    if (changedProperties & TransformChanged)
        encoder << *transform;

    if (changedProperties & SublayerTransformChanged)
        encoder << *sublayerTransform;

    if (changedProperties & HiddenChanged)
        encoder << hidden;

    if (changedProperties & GeometryFlippedChanged)
        encoder << geometryFlipped;

    if (changedProperties & DoubleSidedChanged)
        encoder << doubleSided;

    if (changedProperties & MasksToBoundsChanged)
        encoder << masksToBounds;

    if (changedProperties & OpaqueChanged)
        encoder << opaque;

    if (changedProperties & ContentsHiddenChanged)
        encoder << contentsHidden;

    if (changedProperties & MaskLayerChanged)
        encoder << maskLayerID;

    if (changedProperties & ClonedContentsChanged)
        encoder << clonedLayerID;

    if (changedProperties & ContentsRectChanged)
        encoder << contentsRect;

    if (changedProperties & ContentsScaleChanged)
        encoder << contentsScale;

    if (changedProperties & CornerRadiusChanged)
        encoder << cornerRadius;

    if (changedProperties & ShapeRoundedRectChanged)
        encoder << *shapeRoundedRect;

    if (changedProperties & ShapePathChanged)
        encoder << shapePath;

    if (changedProperties & MinificationFilterChanged)
        encoder.encodeEnum(minificationFilter);

    if (changedProperties & MagnificationFilterChanged)
        encoder.encodeEnum(magnificationFilter);

    if (changedProperties & BlendModeChanged)
        encoder.encodeEnum(blendMode);

    if (changedProperties & WindRuleChanged)
        encoder.encodeEnum(windRule);

    if (changedProperties & SpeedChanged)
        encoder << speed;

    if (changedProperties & TimeOffsetChanged)
        encoder << timeOffset;

    if (changedProperties & BackingStoreChanged) {
        bool hasFrontBuffer = backingStore && backingStore->hasFrontBuffer();
        encoder << hasFrontBuffer;
        if (hasFrontBuffer)
            encoder << *backingStore;
    }

    if (changedProperties & BackingStoreAttachmentChanged)
        encoder << backingStoreAttached;

    if (changedProperties & FiltersChanged)
        encoder << *filters;

    if (changedProperties & EdgeAntialiasingMaskChanged)
        encoder << edgeAntialiasingMask;

    if (changedProperties & CustomAppearanceChanged)
        encoder.encodeEnum(customAppearance);

    if (changedProperties & UserInteractionEnabledChanged)
        encoder << userInteractionEnabled;

    if (changedProperties & EventRegionChanged)
        encoder << eventRegion;
}

bool RemoteLayerTreeTransaction::LayerProperties::decode(IPC::Decoder& decoder, LayerProperties& result)
{
    if (!decoder.decode(result.changedProperties))
        return false;

    if (result.changedProperties & NameChanged) {
        if (!decoder.decode(result.name))
            return false;
    }

    if (result.changedProperties & ChildrenChanged) {
        if (!decoder.decode(result.children))
            return false;

        for (auto& layerID : result.children) {
            if (!layerID)
                return false;
        }
    }

    if (result.changedProperties & AnimationsChanged) {
        if (!decoder.decode(result.addedAnimations))
            return false;

        if (!decoder.decode(result.keyPathsOfAnimationsToRemove))
            return false;
    }

    if (result.changedProperties & PositionChanged) {
        if (!decoder.decode(result.position))
            return false;
    }

    if (result.changedProperties & BoundsChanged) {
        if (!decoder.decode(result.bounds))
            return false;
    }

    if (result.changedProperties & BackgroundColorChanged) {
        if (!decoder.decode(result.backgroundColor))
            return false;
    }

    if (result.changedProperties & AnchorPointChanged) {
        if (!decoder.decode(result.anchorPoint))
            return false;
    }

    if (result.changedProperties & BorderWidthChanged) {
        if (!decoder.decode(result.borderWidth))
            return false;
    }

    if (result.changedProperties & BorderColorChanged) {
        if (!decoder.decode(result.borderColor))
            return false;
    }

    if (result.changedProperties & OpacityChanged) {
        if (!decoder.decode(result.opacity))
            return false;
    }

    if (result.changedProperties & TransformChanged) {
        WebCore::TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;
        
        result.transform = makeUnique<WebCore::TransformationMatrix>(transform);
    }

    if (result.changedProperties & SublayerTransformChanged) {
        WebCore::TransformationMatrix transform;
        if (!decoder.decode(transform))
            return false;

        result.sublayerTransform = makeUnique<WebCore::TransformationMatrix>(transform);
    }

    if (result.changedProperties & HiddenChanged) {
        if (!decoder.decode(result.hidden))
            return false;
    }

    if (result.changedProperties & GeometryFlippedChanged) {
        if (!decoder.decode(result.geometryFlipped))
            return false;
    }

    if (result.changedProperties & DoubleSidedChanged) {
        if (!decoder.decode(result.doubleSided))
            return false;
    }

    if (result.changedProperties & MasksToBoundsChanged) {
        if (!decoder.decode(result.masksToBounds))
            return false;
    }

    if (result.changedProperties & OpaqueChanged) {
        if (!decoder.decode(result.opaque))
            return false;
    }

    if (result.changedProperties & ContentsHiddenChanged) {
        if (!decoder.decode(result.contentsHidden))
            return false;
    }

    if (result.changedProperties & MaskLayerChanged) {
        if (!decoder.decode(result.maskLayerID))
            return false;
    }

    if (result.changedProperties & ClonedContentsChanged) {
        if (!decoder.decode(result.clonedLayerID))
            return false;
    }

    if (result.changedProperties & ContentsRectChanged) {
        if (!decoder.decode(result.contentsRect))
            return false;
    }

    if (result.changedProperties & ContentsScaleChanged) {
        if (!decoder.decode(result.contentsScale))
            return false;
    }

    if (result.changedProperties & CornerRadiusChanged) {
        if (!decoder.decode(result.cornerRadius))
            return false;
    }

    if (result.changedProperties & ShapeRoundedRectChanged) {
        WebCore::FloatRoundedRect roundedRect;
        if (!decoder.decode(roundedRect))
            return false;
        
        result.shapeRoundedRect = makeUnique<WebCore::FloatRoundedRect>(roundedRect);
    }

    if (result.changedProperties & ShapePathChanged) {
        WebCore::Path path;
        if (!decoder.decode(path))
            return false;
        
        result.shapePath = WTFMove(path);
    }

    if (result.changedProperties & MinificationFilterChanged) {
        if (!decoder.decodeEnum(result.minificationFilter))
            return false;
    }

    if (result.changedProperties & MagnificationFilterChanged) {
        if (!decoder.decodeEnum(result.magnificationFilter))
            return false;
    }

    if (result.changedProperties & BlendModeChanged) {
        if (!decoder.decodeEnum(result.blendMode))
            return false;
    }

    if (result.changedProperties & WindRuleChanged) {
        if (!decoder.decodeEnum(result.windRule))
            return false;
    }

    if (result.changedProperties & SpeedChanged) {
        if (!decoder.decode(result.speed))
            return false;
    }

    if (result.changedProperties & TimeOffsetChanged) {
        if (!decoder.decode(result.timeOffset))
            return false;
    }

    if (result.changedProperties & BackingStoreChanged) {
        bool hasFrontBuffer = false;
        if (!decoder.decode(hasFrontBuffer))
            return false;
        if (hasFrontBuffer) {
            auto backingStore = makeUnique<RemoteLayerBackingStore>(nullptr);
            if (!decoder.decode(*backingStore))
                return false;
            
            result.backingStore = WTFMove(backingStore);
        } else
            result.backingStore = nullptr;
    }

    if (result.changedProperties & BackingStoreAttachmentChanged) {
        if (!decoder.decode(result.backingStoreAttached))
            return false;
    }

    if (result.changedProperties & FiltersChanged) {
        auto filters = makeUnique<WebCore::FilterOperations>();
        if (!decoder.decode(*filters))
            return false;
        result.filters = WTFMove(filters);
    }

    if (result.changedProperties & EdgeAntialiasingMaskChanged) {
        if (!decoder.decode(result.edgeAntialiasingMask))
            return false;
    }

    if (result.changedProperties & CustomAppearanceChanged) {
        if (!decoder.decodeEnum(result.customAppearance))
            return false;
    }

    if (result.changedProperties & UserInteractionEnabledChanged) {
        if (!decoder.decode(result.userInteractionEnabled))
            return false;
    }

    if (result.changedProperties & EventRegionChanged) {
        Optional<WebCore::EventRegion> eventRegion;
        decoder >> eventRegion;
        if (!eventRegion)
            return false;
        result.eventRegion = WTFMove(*eventRegion);
    }

    return true;
}

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction()
{
}

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction()
{
}

void RemoteLayerTreeTransaction::encode(IPC::Encoder& encoder) const
{
    encoder << m_rootLayerID;
    encoder << m_createdLayers;

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

    encoder << m_pageExtendedBackgroundColor;
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

    encoder << m_dynamicViewportSizeUpdateID;
}

bool RemoteLayerTreeTransaction::decode(IPC::Decoder& decoder, RemoteLayerTreeTransaction& result)
{
    if (!decoder.decode(result.m_rootLayerID))
        return false;
    if (!result.m_rootLayerID)
        return false;

    if (!decoder.decode(result.m_createdLayers))
        return false;

    uint64_t numChangedLayerProperties;
    if (!decoder.decode(numChangedLayerProperties))
        return false;

    for (uint64_t i = 0; i < numChangedLayerProperties; ++i) {
        WebCore::GraphicsLayer::PlatformLayerID layerID;
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
    
    if (!decoder.decode(result.m_pageExtendedBackgroundColor))
        return false;

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

    Optional<Vector<TransactionCallbackID>> callbackIDs;
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

    if (!decoder.decode(result.m_dynamicViewportSizeUpdateID))
        return false;

    return true;
}

void RemoteLayerTreeTransaction::setRootLayerID(WebCore::GraphicsLayer::PlatformLayerID rootLayerID)
{
    ASSERT_ARG(rootLayerID, rootLayerID);

    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote& remoteLayer)
{
    m_changedLayers.append(&remoteLayer);
}

void RemoteLayerTreeTransaction::setCreatedLayers(Vector<LayerCreationProperties> createdLayers)
{
    m_createdLayers = WTFMove(createdLayers);
}

void RemoteLayerTreeTransaction::setDestroyedLayerIDs(Vector<WebCore::GraphicsLayer::PlatformLayerID> destroyedLayerIDs)
{
    m_destroyedLayerIDs = WTFMove(destroyedLayerIDs);
}

void RemoteLayerTreeTransaction::setLayerIDsWithNewlyUnreachableBackingStore(Vector<WebCore::GraphicsLayer::PlatformLayerID> layerIDsWithNewlyUnreachableBackingStore)
{
    m_layerIDsWithNewlyUnreachableBackingStore = WTFMove(layerIDsWithNewlyUnreachableBackingStore);
}

#if !defined(NDEBUG) || !LOG_DISABLED

static TextStream& operator<<(TextStream& ts, const RemoteLayerBackingStore& backingStore)
{
    ts << backingStore.size();
    ts << " scale=" << backingStore.scale();
    if (backingStore.isOpaque())
        ts << " opaque";
    if (backingStore.acceleratesDrawing())
        ts << " accelerated";
    return ts;
}

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

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::NameChanged)
            ts.dumpProperty("name", layerProperties.name);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ChildrenChanged)
            ts.dumpProperty<Vector<WebCore::GraphicsLayer::PlatformLayerID>>("children", layerProperties.children);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::PositionChanged)
            ts.dumpProperty("position", layerProperties.position);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BoundsChanged)
            ts.dumpProperty("bounds", layerProperties.bounds);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::AnchorPointChanged)
            ts.dumpProperty("anchorPoint", layerProperties.anchorPoint);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackgroundColorChanged)
            ts.dumpProperty("backgroundColor", layerProperties.backgroundColor);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderColorChanged)
            ts.dumpProperty("borderColor", layerProperties.borderColor);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BorderWidthChanged)
            ts.dumpProperty("borderWidth", layerProperties.borderWidth);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpacityChanged)
            ts.dumpProperty("opacity", layerProperties.opacity);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TransformChanged)
            ts.dumpProperty("transform", layerProperties.transform ? *layerProperties.transform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SublayerTransformChanged)
            ts.dumpProperty("sublayerTransform", layerProperties.sublayerTransform ? *layerProperties.sublayerTransform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::HiddenChanged)
            ts.dumpProperty("hidden", layerProperties.hidden);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::GeometryFlippedChanged)
            ts.dumpProperty("geometryFlipped", layerProperties.geometryFlipped);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::DoubleSidedChanged)
            ts.dumpProperty("doubleSided", layerProperties.doubleSided);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MasksToBoundsChanged)
            ts.dumpProperty("masksToBounds", layerProperties.masksToBounds);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::OpaqueChanged)
            ts.dumpProperty("opaque", layerProperties.opaque);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsHiddenChanged)
            ts.dumpProperty("contentsHidden", layerProperties.contentsHidden);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MaskLayerChanged)
            ts.dumpProperty("maskLayer", layerProperties.maskLayerID);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ClonedContentsChanged)
            ts.dumpProperty("clonedLayer", layerProperties.clonedLayerID);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsRectChanged)
            ts.dumpProperty("contentsRect", layerProperties.contentsRect);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ContentsScaleChanged)
            ts.dumpProperty("contentsScale", layerProperties.contentsScale);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::CornerRadiusChanged)
            ts.dumpProperty("cornerRadius", layerProperties.cornerRadius);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::ShapeRoundedRectChanged)
            ts.dumpProperty("shapeRect", layerProperties.shapeRoundedRect ? *layerProperties.shapeRoundedRect : WebCore::FloatRoundedRect());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MinificationFilterChanged)
            ts.dumpProperty("minificationFilter", layerProperties.minificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::MagnificationFilterChanged)
            ts.dumpProperty("magnificationFilter", layerProperties.magnificationFilter);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BlendModeChanged)
            ts.dumpProperty("blendMode", layerProperties.blendMode);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::SpeedChanged)
            ts.dumpProperty("speed", layerProperties.speed);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::TimeOffsetChanged)
            ts.dumpProperty("timeOffset", layerProperties.timeOffset);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackingStoreChanged) {
            if (const RemoteLayerBackingStore* backingStore = layerProperties.backingStore.get())
                ts.dumpProperty<const RemoteLayerBackingStore&>("backingStore", *backingStore);
            else
                ts.dumpProperty("backingStore", "removed");
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::BackingStoreAttachmentChanged)
            ts.dumpProperty("backingStoreAttached", layerProperties.backingStoreAttached);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::FiltersChanged)
            ts.dumpProperty("filters", layerProperties.filters ? *layerProperties.filters : WebCore::FilterOperations());

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::AnimationsChanged) {
            for (const auto& keyAnimationPair : layerProperties.addedAnimations)
                ts.dumpProperty("animation " +  keyAnimationPair.first, keyAnimationPair.second);

            for (const auto& name : layerProperties.keyPathsOfAnimationsToRemove)
                ts.dumpProperty("removed animation", name);
        }

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::EdgeAntialiasingMaskChanged)
            ts.dumpProperty("edgeAntialiasingMask", layerProperties.edgeAntialiasingMask);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::CustomAppearanceChanged)
            ts.dumpProperty("customAppearance", layerProperties.customAppearance);

        if (layerProperties.changedProperties & RemoteLayerTreeTransaction::UserInteractionEnabledChanged)
            ts.dumpProperty("userInteractionEnabled", layerProperties.userInteractionEnabled);
    }
}

void RemoteLayerTreeTransaction::dump() const
{
    fprintf(stderr, "%s", description().data());
}

CString RemoteLayerTreeTransaction::description() const
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
            case WebCore::PlatformCALayer::LayerTypeContentsProvidedLayer:
                ts << " (context-id " << createdLayer.hostingContextID << ")";
                break;
            case WebCore::PlatformCALayer::LayerTypeCustom:
                ts << " (context-id " << createdLayer.hostingContextID << ")";
                break;
            default:
                break;
            }
        }
    }

    dumpChangedLayers(ts, m_changedLayerProperties);

    if (!m_destroyedLayerIDs.isEmpty())
        ts.dumpProperty<Vector<WebCore::GraphicsLayer::PlatformLayerID>>("destroyed-layers", m_destroyedLayerIDs);

    if (m_editorState) {
        TextStream::GroupScope scope(ts);
        ts << "EditorState";
        ts << *m_editorState;
    }

    ts.endGroup();

    return ts.release().utf8();
}

#endif // !defined(NDEBUG) || !LOG_DISABLED

} // namespace WebKit
