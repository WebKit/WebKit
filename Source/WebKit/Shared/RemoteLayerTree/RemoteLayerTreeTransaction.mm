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
#import "LayerProperties.h"
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

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction() = default;

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction() = default;

void RemoteLayerTreeTransaction::setRootLayerID(WebCore::PlatformLayerIdentifier rootLayerID)
{
    ASSERT_ARG(rootLayerID, rootLayerID);

    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote& remoteLayer)
{
    ASSERT(WebCore::isInWebProcess());
    m_changedLayers.changedLayers.add(remoteLayer);
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

static void dumpChangedLayers(TextStream& ts, const LayerPropertiesMap& changedLayerProperties)
{
    if (changedLayerProperties.isEmpty())
        return;

    TextStream::GroupScope group(ts);
    ts << "changed-layers";

    // Dump the layer properties sorted by layer ID.
    auto layerIDs = copyToVector(changedLayerProperties.keys());
    std::sort(layerIDs.begin(), layerIDs.end());

    for (auto& layerID : layerIDs) {
        const auto& layerProperties = *changedLayerProperties.get(layerID);

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
            if (auto* backingStoreProperties = layerProperties.backingStoreOrProperties.properties.get())
                ts.dumpProperty("backingStore", *backingStoreProperties);
            else
                ts.dumpProperty("backingStore", "removed");
        }

        if (layerProperties.changedProperties & LayerChange::BackingStoreAttachmentChanged)
            ts.dumpProperty("backingStoreAttached", layerProperties.backingStoreAttached);

        if (layerProperties.changedProperties & LayerChange::FiltersChanged)
            ts.dumpProperty("filters", layerProperties.filters ? *layerProperties.filters : WebCore::FilterOperations());

        if (layerProperties.changedProperties & LayerChange::AnimationsChanged) {
            for (const auto& keyAnimationPair : layerProperties.animationChanges.addedAnimations)
                ts.dumpProperty("animation " +  keyAnimationPair.first, keyAnimationPair.second);

            for (const auto& name : layerProperties.animationChanges.keysOfAnimationsToRemove)
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

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        if (layerProperties.changedProperties & LayerChange::VisibleRectChanged)
            ts.dumpProperty("visibleRect", layerProperties.visibleRect);
#endif
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
            case WebCore::PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
                ts << " (context-id " << createdLayer.hostingContextID() << ")";
                break;
            case WebCore::PlatformCALayer::LayerType::LayerTypeCustom:
                ts << " (context-id " << createdLayer.hostingContextID() << ")";
                break;
#if ENABLE(MODEL_ELEMENT)
            case WebCore::PlatformCALayer::LayerType::LayerTypeModelLayer:
                if (auto* model = std::get_if<Ref<WebCore::Model>>(&createdLayer.additionalData))
                    ts << " (model " << model->get() << ")";
                break;
#endif
            default:
                break;
            }
        }
    }

    dumpChangedLayers(ts, m_changedLayers.changedLayerProperties);

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

bool RemoteLayerTreeTransaction::hasAnyLayerChanges() const
{
    return m_changedLayers.changedLayers.size()
        || m_changedLayers.changedLayerProperties.size()
        || m_createdLayers.size()
        || m_destroyedLayerIDs.size()
        || m_layerIDsWithNewlyUnreachableBackingStore.size();
}

HashSet<Ref<PlatformCALayerRemote>>& RemoteLayerTreeTransaction::changedLayers()
{
    ASSERT(WebCore::isInWebProcess());
    return m_changedLayers.changedLayers;
}

const LayerPropertiesMap& RemoteLayerTreeTransaction::changedLayerProperties() const
{
    ASSERT(!isInAuxiliaryProcess());
    return m_changedLayers.changedLayerProperties;
}

LayerPropertiesMap& RemoteLayerTreeTransaction::changedLayerProperties()
{
    ASSERT(!isInAuxiliaryProcess());
    return m_changedLayers.changedLayerProperties;
}

ChangedLayers::ChangedLayers() = default;

ChangedLayers::~ChangedLayers() = default;

ChangedLayers::ChangedLayers(ChangedLayers&&) = default;

ChangedLayers& ChangedLayers::operator=(ChangedLayers&&) = default;

ChangedLayers::ChangedLayers(LayerPropertiesMap&& changedLayerProperties)
    : changedLayerProperties(WTFMove(changedLayerProperties)) { }

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties() = default;

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties(LayerCreationProperties&&) = default;

auto RemoteLayerTreeTransaction::LayerCreationProperties::operator=(LayerCreationProperties&&) -> LayerCreationProperties& = default;

RemoteLayerBackingStoreOrProperties::~RemoteLayerBackingStoreOrProperties() = default;

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties(WebCore::PlatformLayerIdentifier layerID, WebCore::PlatformCALayer::LayerType type, std::optional<RemoteLayerTreeTransaction::LayerCreationProperties::VideoElementData>&& videoElementData, RemoteLayerTreeTransaction::LayerCreationProperties::AdditionalData&& additionalData)
    : layerID(layerID)
    , type(type)
    , videoElementData(WTFMove(videoElementData))
    , additionalData(WTFMove(additionalData)) { }

std::optional<WebCore::LayerHostingContextIdentifier> RemoteLayerTreeTransaction::LayerCreationProperties::hostIdentifier() const
{
    if (auto* identifier = std::get_if<WebCore::LayerHostingContextIdentifier>(&additionalData))
        return *identifier;
    return std::nullopt;
}

uint32_t RemoteLayerTreeTransaction::LayerCreationProperties::hostingContextID() const
{
    if (auto* customData = std::get_if<CustomData>(&additionalData))
        return customData->hostingContextID;
    return 0;
}

bool RemoteLayerTreeTransaction::LayerCreationProperties::preservesFlip() const
{
    if (auto* customData = std::get_if<CustomData>(&additionalData))
        return customData->preservesFlip;
    return false;
}

float RemoteLayerTreeTransaction::LayerCreationProperties::hostingDeviceScaleFactor() const
{
    if (auto* customData = std::get_if<CustomData>(&additionalData))
        return customData->hostingDeviceScaleFactor;
    return 1;
}

} // namespace WebKit

namespace IPC {

void ArgumentCoder<WebKit::ChangedLayers>::encode(Encoder& encoder, const WebKit::ChangedLayers& instance)
{
    // Although the data is not stored as a LayerPropertiesMap in the web content process, we want it to
    // decode as a LayerPropertiesMap in the UI process without doing any unnecessary transformations or allocations.
    ASSERT(WebCore::isInWebProcess());
    encoder << instance.changedLayers.size();
    for (const auto& layer : instance.changedLayers) {
        encoder << layer->layerID();
        encoder << layer->properties();
    }
}

void ArgumentCoder<WebKit::RemoteLayerBackingStoreOrProperties>::encode(Encoder& encoder, const WebKit::RemoteLayerBackingStoreOrProperties& instance)
{
    // The web content process has a std::unique_ptr<RemoteLayerBackingStore> but we want it to decode
    // in the UI process as a std::unique_ptr<RemoteLayerBackingStoreProperties>.
    ASSERT(WebCore::isInWebProcess());
    bool hasFrontBuffer = instance.store && instance.store->hasFrontBuffer();
    encoder << hasFrontBuffer;
    if (hasFrontBuffer)
        encoder << *instance.store;
}

}
