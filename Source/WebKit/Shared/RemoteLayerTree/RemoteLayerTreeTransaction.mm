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
#import <QuartzCore/QuartzCore.h>
#import <WebCore/EventRegion.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/Model.h>
#import <WebCore/TimingFunction.h>
#import <ranges>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/CString.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>

#if ENABLE(MODEL_PROCESS)
#import <WebCore/ModelContext.h>
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteLayerTreeTransaction);

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&) = default;

RemoteLayerTreeTransaction& RemoteLayerTreeTransaction::operator=(RemoteLayerTreeTransaction&&) = default;

RemoteLayerTreeTransaction::RemoteLayerTreeTransaction() = default;

RemoteLayerTreeTransaction::~RemoteLayerTreeTransaction() = default;

void RemoteLayerTreeTransaction::setRootLayerID(WebCore::PlatformLayerIdentifier rootLayerID)
{
    m_rootLayerID = rootLayerID;
}

void RemoteLayerTreeTransaction::layerPropertiesChanged(PlatformCALayerRemote& remoteLayer)
{
    ASSERT(isInWebProcess());
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
    ts << "changed-layers"_s;

    // Dump the layer properties sorted by layer ID.
    auto layerIDs = copyToVector(changedLayerProperties.keys());
    std::ranges::sort(layerIDs, { }, &WebCore::PlatformLayerIdentifier::object);

    for (auto& layerID : layerIDs) {
        const auto& layerProperties = *changedLayerProperties.get(layerID);

        TextStream::GroupScope group(ts);
        ts << "layer "_s << layerID;

        if (layerProperties.changedProperties & LayerChange::NameChanged)
            ts.dumpProperty("name"_s, layerProperties.name);

        if (layerProperties.changedProperties & LayerChange::ChildrenChanged)
            ts.dumpProperty<Vector<WebCore::PlatformLayerIdentifier>>("children", layerProperties.children);

        if (layerProperties.changedProperties & LayerChange::PositionChanged)
            ts.dumpProperty("position"_s, layerProperties.position);

        if (layerProperties.changedProperties & LayerChange::BoundsChanged)
            ts.dumpProperty("bounds"_s, layerProperties.bounds);

        if (layerProperties.changedProperties & LayerChange::AnchorPointChanged)
            ts.dumpProperty("anchorPoint"_s, layerProperties.anchorPoint);

        if (layerProperties.changedProperties & LayerChange::BackgroundColorChanged)
            ts.dumpProperty("backgroundColor"_s, layerProperties.backgroundColor);

        if (layerProperties.changedProperties & LayerChange::BorderColorChanged)
            ts.dumpProperty("borderColor"_s, layerProperties.borderColor);

        if (layerProperties.changedProperties & LayerChange::BorderWidthChanged)
            ts.dumpProperty("borderWidth"_s, layerProperties.borderWidth);

        if (layerProperties.changedProperties & LayerChange::OpacityChanged)
            ts.dumpProperty("opacity"_s, layerProperties.opacity);

        if (layerProperties.changedProperties & LayerChange::TransformChanged)
            ts.dumpProperty("transform"_s, layerProperties.transform ? *layerProperties.transform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & LayerChange::SublayerTransformChanged)
            ts.dumpProperty("sublayerTransform"_s, layerProperties.sublayerTransform ? *layerProperties.sublayerTransform : WebCore::TransformationMatrix());

        if (layerProperties.changedProperties & LayerChange::HiddenChanged)
            ts.dumpProperty("hidden"_s, layerProperties.hidden);

        if (layerProperties.changedProperties & LayerChange::GeometryFlippedChanged)
            ts.dumpProperty("geometryFlipped"_s, layerProperties.geometryFlipped);

        if (layerProperties.changedProperties & LayerChange::DoubleSidedChanged)
            ts.dumpProperty("doubleSided"_s, layerProperties.doubleSided);

        if (layerProperties.changedProperties & LayerChange::MasksToBoundsChanged)
            ts.dumpProperty("masksToBounds"_s, layerProperties.masksToBounds);

        if (layerProperties.changedProperties & LayerChange::ContentsHiddenChanged)
            ts.dumpProperty("contentsHidden"_s, layerProperties.contentsHidden);

        if (layerProperties.changedProperties & LayerChange::MaskLayerChanged)
            ts.dumpProperty("maskLayer"_s, layerProperties.maskLayerID);

        if (layerProperties.changedProperties & LayerChange::ClonedContentsChanged)
            ts.dumpProperty("clonedLayer"_s, layerProperties.clonedLayerID);

#if ENABLE(SCROLLING_THREAD)
        if (layerProperties.changedProperties & LayerChange::ScrollingNodeIDChanged)
            ts.dumpProperty("scrollingNodeID"_s, layerProperties.scrollingNodeID);
#endif

        if (layerProperties.changedProperties & LayerChange::ContentsRectChanged)
            ts.dumpProperty("contentsRect"_s, layerProperties.contentsRect);

        if (layerProperties.changedProperties & LayerChange::ContentsScaleChanged)
            ts.dumpProperty("contentsScale"_s, layerProperties.contentsScale);

        if (layerProperties.changedProperties & LayerChange::CornerRadiusChanged)
            ts.dumpProperty("cornerRadius"_s, layerProperties.cornerRadius);

        if (layerProperties.changedProperties & LayerChange::ShapeRoundedRectChanged)
            ts.dumpProperty("shapeRect"_s, layerProperties.shapeRoundedRect ? *layerProperties.shapeRoundedRect : WebCore::FloatRoundedRect());

        if (layerProperties.changedProperties & LayerChange::MinificationFilterChanged)
            ts.dumpProperty("minificationFilter"_s, layerProperties.minificationFilter);

        if (layerProperties.changedProperties & LayerChange::MagnificationFilterChanged)
            ts.dumpProperty("magnificationFilter"_s, layerProperties.magnificationFilter);

        if (layerProperties.changedProperties & LayerChange::BlendModeChanged)
            ts.dumpProperty("blendMode"_s, layerProperties.blendMode);

        if (layerProperties.changedProperties & LayerChange::SpeedChanged)
            ts.dumpProperty("speed"_s, layerProperties.speed);

        if (layerProperties.changedProperties & LayerChange::TimeOffsetChanged)
            ts.dumpProperty("timeOffset"_s, layerProperties.timeOffset);

        if (layerProperties.changedProperties & LayerChange::BackingStoreChanged) {
            if (auto* backingStoreProperties = layerProperties.backingStoreOrProperties.properties.get())
                ts.dumpProperty("backingStore"_s, *backingStoreProperties);
            else
                ts.dumpProperty("backingStore"_s, "removed");
        }

        if (layerProperties.changedProperties & LayerChange::BackingStoreAttachmentChanged)
            ts.dumpProperty("backingStoreAttached"_s, layerProperties.backingStoreAttached);

        if (layerProperties.changedProperties & LayerChange::FiltersChanged)
            ts.dumpProperty("filters"_s, layerProperties.filters ? *layerProperties.filters : WebCore::FilterOperations());

        if (layerProperties.changedProperties & LayerChange::AnimationsChanged) {
            for (const auto& keyAnimationPair : layerProperties.animationChanges.addedAnimations)
                ts.dumpProperty(makeString("animation "_s, keyAnimationPair.first), keyAnimationPair.second);

            for (const auto& name : layerProperties.animationChanges.keysOfAnimationsToRemove)
                ts.dumpProperty("removed animation"_s, name);
        }

        if (layerProperties.changedProperties & LayerChange::AntialiasesEdgesChanged)
            ts.dumpProperty("antialiasesEdges"_s, layerProperties.antialiasesEdges);

        if (layerProperties.changedProperties & LayerChange::CustomAppearanceChanged)
            ts.dumpProperty("customAppearance"_s, layerProperties.customAppearance);

        if (layerProperties.changedProperties & LayerChange::UserInteractionEnabledChanged)
            ts.dumpProperty("userInteractionEnabled"_s, layerProperties.userInteractionEnabled);

        if (layerProperties.changedProperties & LayerChange::EventRegionChanged)
            ts.dumpProperty("eventRegion"_s, layerProperties.eventRegion);

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        if (layerProperties.changedProperties & LayerChange::VisibleRectChanged)
            ts.dumpProperty("visibleRect"_s, layerProperties.visibleRect);
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        if (layerProperties.changedProperties & LayerChange::SeparatedChanged)
            ts.dumpProperty("isSeparated"_s, layerProperties.isSeparated);

#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
        if (layerProperties.changedProperties & LayerChange::SeparatedPortalChanged)
            ts.dumpProperty("isSeparatedPortal"_s, layerProperties.isSeparatedPortal);

        if (layerProperties.changedProperties & LayerChange::DescendentOfSeparatedPortalChanged)
            ts.dumpProperty("isDescendentOfSeparatedPortal"_s, layerProperties.isDescendentOfSeparatedPortal);
#endif
#endif

        if (layerProperties.changedProperties & LayerChange::VideoGravityChanged)
            ts.dumpProperty("videoGravity"_s, layerProperties.videoGravity);

#if HAVE(CORE_MATERIAL)
        if (layerProperties.changedProperties & LayerChange::AppleVisualEffectChanged)
            ts.dumpProperty("appleVisualEffectData"_s, layerProperties.appleVisualEffectData);
#endif
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
    ts << "layer tree"_s;

    ts.dumpProperty("contentsSize"_s, m_contentsSize);
    ts.dumpProperty("scrollGeometryContentSize"_s, m_scrollGeometryContentSize);
    if (m_scrollOrigin != WebCore::IntPoint::zero())
        ts.dumpProperty("scrollOrigin"_s, m_scrollOrigin);

    ts.dumpProperty("root-layer"_s, m_rootLayerID);

    if (!m_createdLayers.isEmpty()) {
        TextStream::GroupScope group(ts);
        ts << "created-layers"_s;
        for (const auto& createdLayer : m_createdLayers) {
            TextStream::GroupScope group(ts);
            ts << createdLayer.type <<" " << createdLayer.layerID;
            switch (createdLayer.type) {
            case WebCore::PlatformCALayer::LayerType::LayerTypeAVPlayerLayer:
                ts << " (context-id "_s << createdLayer.hostingContextID() << ')';
                break;
            case WebCore::PlatformCALayer::LayerType::LayerTypeCustom:
                ts << " (context-id "_s << createdLayer.hostingContextID() << ')';
                break;
#if ENABLE(MODEL_ELEMENT)
            case WebCore::PlatformCALayer::LayerType::LayerTypeModelLayer:
                if (auto* model = std::get_if<Ref<WebCore::Model>>(&createdLayer.additionalData))
                    ts << " (model "_s << model->get() << ')';
                break;
#endif
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
            case WebCore::PlatformCALayer::LayerType::LayerTypeSeparatedImageLayer:
                ts << " (separated image)"_s;
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
    ASSERT(isInWebProcess());
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

RemoteLayerTreeTransaction::LayerCreationProperties::LayerCreationProperties(Markable<WebCore::PlatformLayerIdentifier> layerID, WebCore::PlatformCALayer::LayerType type, std::optional<RemoteLayerTreeTransaction::LayerCreationProperties::VideoElementData>&& videoElementData, RemoteLayerTreeTransaction::LayerCreationProperties::AdditionalData&& additionalData)
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
#if ENABLE(MODEL_PROCESS)
    if (auto* modelContext = std::get_if<Ref<WebCore::ModelContext>>(&additionalData))
        return (*modelContext)->modelContentsLayerHostingContextIdentifier().toRawValue();
#endif

    if (auto* customData = std::get_if<CustomData>(&additionalData))
        return customData->hostingContextID;
    return 0;
}

#if ENABLE(MACH_PORT_LAYER_HOSTING)
std::optional<WTF::MachSendRightAnnotated> RemoteLayerTreeTransaction::LayerCreationProperties::sendRightAnnotated() const
{
    if (auto* customData = std::get_if<CustomData>(&additionalData))
        return customData->sendRightAnnotated;
    return std::nullopt;
}
#endif

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

#if ENABLE(MODEL_PROCESS)
RefPtr<WebCore::ModelContext> RemoteLayerTreeTransaction::LayerCreationProperties::modelContext() const
{
    auto* modelContext = std::get_if<Ref<WebCore::ModelContext>>(&additionalData);
    if (!modelContext)
        return nullptr;
    return modelContext->ptr();
}
#endif

} // namespace WebKit

namespace IPC {

void ArgumentCoder<WebKit::ChangedLayers>::encode(Encoder& encoder, const WebKit::ChangedLayers& instance)
{
    // Although the data is not stored as a LayerPropertiesMap in the web content process, we want it to
    // decode as a LayerPropertiesMap in the UI process without doing any unnecessary transformations or allocations.
    ASSERT(isInWebProcess());
    encoder << instance.changedLayers.size();
    for (const auto& layer : instance.changedLayers) {
        encoder << layer->layerID();
        encoder << layer->properties();
    }
}

template<> struct ArgumentCoder<WebKit::RemoteLayerBackingStore> {
    static void encode(Encoder& encoder, const WebKit::RemoteLayerBackingStore& store)
    {
        store.encode(encoder);
    }
    // This intentionally has no decode because it is only decoded as a RemoteLayerBackingStoreProperties.
};

void ArgumentCoder<WebKit::RemoteLayerBackingStoreOrProperties>::encode(Encoder& encoder, const WebKit::RemoteLayerBackingStoreOrProperties& instance)
{
    // The web content process has a std::unique_ptr<RemoteLayerBackingStore> but we want it to decode
    // in the UI process as a std::unique_ptr<RemoteLayerBackingStoreProperties>.
    ASSERT(isInWebProcess());
    CheckedPtr store = instance.store.get();
    bool hasFrontBuffer = store && store->hasFrontBuffer();
    encoder << hasFrontBuffer;
    if (hasFrontBuffer)
        encoder << *store;
}

}
