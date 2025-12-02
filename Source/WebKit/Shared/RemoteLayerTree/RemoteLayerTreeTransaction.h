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

#pragma once

#include "DrawingAreaInfo.h"
#include "PlatformCAAnimationRemote.h"
#include "PlaybackSessionContextIdentifier.h"
#include "RemoteLayerBackingStore.h"
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatSize.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if ENABLE(THREADED_ANIMATIONS)
#include <WebCore/AcceleratedTimeline.h>
#endif

#if ENABLE(MODEL_ELEMENT)
#include <WebCore/Model.h>
#endif

#if ENABLE(MODEL_PROCESS)
#include <WebCore/ModelContext.h>
#endif

#if ENABLE(MACH_PORT_LAYER_HOSTING)
#include <wtf/MachSendRightAnnotated.h>
#endif

namespace WebKit {

struct LayerProperties;
typedef HashMap<WebCore::PlatformLayerIdentifier, UniqueRef<LayerProperties>> LayerPropertiesMap;

struct ChangedLayers {
    HashSet<Ref<PlatformCALayerRemote>> changedLayers; // Only used in the Web process.
    LayerPropertiesMap changedLayerProperties; // Only used in the UI process.

    ChangedLayers();
    ChangedLayers(ChangedLayers&&);
    ChangedLayers& operator=(ChangedLayers&&);
    ChangedLayers(LayerPropertiesMap&&);
    ~ChangedLayers();
};

class RemoteLayerTreeTransaction final : public CanMakeCheckedPtr<RemoteLayerTreeTransaction, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeTransaction);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteLayerTreeTransaction);
public:
    struct LayerCreationProperties {
        struct NoAdditionalData { };
        struct CustomData {
            uint32_t hostingContextID { 0 };
#if ENABLE(MACH_PORT_LAYER_HOSTING)
            std::optional<WTF::MachSendRightAnnotated> sendRightAnnotated;
#endif
            float hostingDeviceScaleFactor { 1 };
            bool preservesFlip { false };
        };
        struct VideoElementData {
            PlaybackSessionContextIdentifier playerIdentifier;
            WebCore::FloatSize initialSize;
            WebCore::FloatSize naturalSize;
        };
        using AdditionalData = Variant<
            NoAdditionalData, // PlatformCALayerRemote and PlatformCALayerRemoteTiledBacking
            CustomData, // PlatformCALayerRemoteCustom
#if ENABLE(MODEL_ELEMENT)
            Ref<WebCore::Model>, // PlatformCALayerRemoteModelHosting
#if ENABLE(MODEL_PROCESS)
            Ref<WebCore::ModelContext>, // PlatformCALayerRemoteCustom
#endif
#endif
            WebCore::LayerHostingContextIdentifier // PlatformCALayerRemoteHost
        >;

        Markable<WebCore::PlatformLayerIdentifier> layerID;
        WebCore::PlatformCALayer::LayerType type { WebCore::PlatformCALayer::LayerType::LayerTypeLayer };
        std::optional<VideoElementData> videoElementData;
        AdditionalData additionalData;

        LayerCreationProperties();
        LayerCreationProperties(Markable<WebCore::PlatformLayerIdentifier>, WebCore::PlatformCALayer::LayerType, std::optional<VideoElementData>&&, AdditionalData&&);
        LayerCreationProperties(LayerCreationProperties&&);
        LayerCreationProperties& operator=(LayerCreationProperties&&);

        std::optional<WebCore::LayerHostingContextIdentifier> hostIdentifier() const;
        uint32_t hostingContextID() const;
#if ENABLE(MACH_PORT_LAYER_HOSTING)
        std::optional<WTF::MachSendRightAnnotated> sendRightAnnotated() const;
#endif
        bool preservesFlip() const;
        float hostingDeviceScaleFactor() const;

#if ENABLE(MODEL_PROCESS)
        RefPtr<WebCore::ModelContext> modelContext() const;
#endif
    };

    RemoteLayerTreeTransaction();
    ~RemoteLayerTreeTransaction();
    RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&);
    RemoteLayerTreeTransaction& operator=(RemoteLayerTreeTransaction&&);

    std::optional<WebCore::PlatformLayerIdentifier> rootLayerID() const { return m_rootLayerID.asOptional(); }
    void setRootLayerID(WebCore::PlatformLayerIdentifier);
    void layerPropertiesChanged(PlatformCALayerRemote&);
    void setCreatedLayers(Vector<LayerCreationProperties>);
    void setDestroyedLayerIDs(Vector<WebCore::PlatformLayerIdentifier>);
    void setLayerIDsWithNewlyUnreachableBackingStore(Vector<WebCore::PlatformLayerIdentifier>);

#if !defined(NDEBUG) || !LOG_DISABLED
    String description() const;
    void dump() const;
#endif
    
    bool hasAnyLayerChanges() const;

    const Vector<LayerCreationProperties>& createdLayers() const { return m_createdLayers; }
    const Vector<WebCore::PlatformLayerIdentifier>& destroyedLayers() const { return m_destroyedLayerIDs; }
    const Vector<WebCore::PlatformLayerIdentifier>& layerIDsWithNewlyUnreachableBackingStore() const { return m_layerIDsWithNewlyUnreachableBackingStore; }

    HashSet<Ref<PlatformCALayerRemote>>& changedLayers();

    const LayerPropertiesMap& changedLayerProperties() const;
    LayerPropertiesMap& changedLayerProperties();

    void setRemoteContextHostedIdentifier(Markable<WebCore::LayerHostingContextIdentifier> identifier) { m_remoteContextHostedIdentifier = identifier; }
    Markable<WebCore::LayerHostingContextIdentifier> remoteContextHostedIdentifier() const { return m_remoteContextHostedIdentifier; }

    WebCore::IntSize contentsSize() const { return m_contentsSize; }
    void setContentsSize(const WebCore::IntSize& size) { m_contentsSize = size; };

    WebCore::IntSize scrollGeometryContentSize() const { return m_scrollGeometryContentSize; }
    void setScrollGeometryContentSize(const WebCore::IntSize& size) { m_scrollGeometryContentSize = size; };

    WebCore::IntPoint scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const WebCore::IntPoint& origin) { m_scrollOrigin = origin; };

    WebCore::IntPoint scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(WebCore::IntPoint p) { m_scrollPosition = p; }

#if ENABLE(THREADED_ANIMATIONS)
    const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelines() const { return m_timelines; }
    void setTimelines(const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelines) { m_timelines = timelines; }
#endif

private:
    friend struct IPC::ArgumentCoder<RemoteLayerTreeTransaction>;

    Markable<WebCore::PlatformLayerIdentifier> m_rootLayerID;
    ChangedLayers m_changedLayers;

    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostedIdentifier;

    Vector<LayerCreationProperties> m_createdLayers;
    Vector<WebCore::PlatformLayerIdentifier> m_destroyedLayerIDs;
    Vector<WebCore::PlatformLayerIdentifier> m_layerIDsWithNewlyUnreachableBackingStore;

    WebCore::IntSize m_contentsSize;
    WebCore::IntSize m_scrollGeometryContentSize;
    WebCore::IntPoint m_scrollOrigin;
    WebCore::IntPoint m_scrollPosition;

#if ENABLE(THREADED_ANIMATIONS)
    HashSet<Ref<WebCore::AcceleratedTimeline>> m_timelines;
#endif
};

} // namespace WebKit
