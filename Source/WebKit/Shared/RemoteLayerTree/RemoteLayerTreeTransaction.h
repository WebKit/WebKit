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

#include "Connection.h"
#include "DrawingAreaInfo.h"
#include "EditorState.h"
#include "PlatformCAAnimationRemote.h"
#include "PlaybackSessionContextIdentifier.h"
#include "RemoteLayerBackingStore.h"
#include "TransactionID.h"
#include <WebCore/Color.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatSize.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include "DynamicViewportSizeUpdate.h"
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include <WebCore/AcceleratedEffect.h>
#include <WebCore/AcceleratedEffectValues.h>
#endif

#if ENABLE(MODEL_ELEMENT)
#include <WebCore/Model.h>
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

class RemoteLayerTreeTransaction {
    WTF_MAKE_TZONE_ALLOCATED(RemoteLayerTreeTransaction);
public:
    struct LayerCreationProperties {
        struct NoAdditionalData { };
        struct CustomData {
            uint32_t hostingContextID { 0 };
            float hostingDeviceScaleFactor { 1 };
            bool preservesFlip { false };
        };
        struct VideoElementData {
            PlaybackSessionContextIdentifier playerIdentifier;
            WebCore::FloatSize initialSize;
            WebCore::FloatSize naturalSize;
        };
        using AdditionalData = std::variant<
            NoAdditionalData, // PlatformCALayerRemote and PlatformCALayerRemoteTiledBacking
            CustomData, // PlatformCALayerRemoteCustom
#if ENABLE(MODEL_ELEMENT)
            Ref<WebCore::Model>, // PlatformCALayerRemoteModelHosting
#endif
            WebCore::LayerHostingContextIdentifier // PlatformCALayerRemoteHost
        >;

        WebCore::PlatformLayerIdentifier layerID;
        WebCore::PlatformCALayer::LayerType type { WebCore::PlatformCALayer::LayerType::LayerTypeLayer };
        std::optional<VideoElementData> videoElementData;
        AdditionalData additionalData;

        LayerCreationProperties();
        LayerCreationProperties(WebCore::PlatformLayerIdentifier, WebCore::PlatformCALayer::LayerType, std::optional<VideoElementData>&&, AdditionalData&&);
        LayerCreationProperties(LayerCreationProperties&&);
        LayerCreationProperties& operator=(LayerCreationProperties&&);

        std::optional<WebCore::LayerHostingContextIdentifier> hostIdentifier() const;
        uint32_t hostingContextID() const;
        bool preservesFlip() const;
        float hostingDeviceScaleFactor() const;
    };

    explicit RemoteLayerTreeTransaction();
    ~RemoteLayerTreeTransaction();
    RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&);
    RemoteLayerTreeTransaction& operator=(RemoteLayerTreeTransaction&&);

    WebCore::PlatformLayerIdentifier rootLayerID() const { return m_rootLayerID; }
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
    bool isMainFrameProcessTransaction() const { return !m_remoteContextHostedIdentifier; }

    WebCore::IntSize contentsSize() const { return m_contentsSize; }
    void setContentsSize(const WebCore::IntSize& size) { m_contentsSize = size; };

    WebCore::IntPoint scrollOrigin() const { return m_scrollOrigin; }
    void setScrollOrigin(const WebCore::IntPoint& origin) { m_scrollOrigin = origin; };

    WebCore::LayoutSize baseLayoutViewportSize() const { return m_baseLayoutViewportSize; }
    void setBaseLayoutViewportSize(const WebCore::LayoutSize& size) { m_baseLayoutViewportSize = size; };
    
    WebCore::LayoutPoint minStableLayoutViewportOrigin() const { return m_minStableLayoutViewportOrigin; }
    void setMinStableLayoutViewportOrigin(const WebCore::LayoutPoint& point) { m_minStableLayoutViewportOrigin = point; };
    
    WebCore::LayoutPoint maxStableLayoutViewportOrigin() const { return m_maxStableLayoutViewportOrigin; }
    void setMaxStableLayoutViewportOrigin(const WebCore::LayoutPoint& point) { m_maxStableLayoutViewportOrigin = point; };

    WebCore::Color themeColor() const { return m_themeColor; }
    void setThemeColor(WebCore::Color color) { m_themeColor = color; }

    WebCore::Color pageExtendedBackgroundColor() const { return m_pageExtendedBackgroundColor; }
    void setPageExtendedBackgroundColor(WebCore::Color color) { m_pageExtendedBackgroundColor = color; }

    WebCore::Color sampledPageTopColor() const { return m_sampledPageTopColor; }
    void setSampledPageTopColor(WebCore::Color color) { m_sampledPageTopColor = color; }

    WebCore::IntPoint scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(WebCore::IntPoint p) { m_scrollPosition = p; }

    double pageScaleFactor() const { return m_pageScaleFactor; }
    void setPageScaleFactor(double pageScaleFactor) { m_pageScaleFactor = pageScaleFactor; }

    bool scaleWasSetByUIProcess() const { return m_scaleWasSetByUIProcess; }
    void setScaleWasSetByUIProcess(bool scaleWasSetByUIProcess) { m_scaleWasSetByUIProcess = scaleWasSetByUIProcess; }

#if PLATFORM(MAC)
    WebCore::PlatformLayerIdentifier pageScalingLayerID() const { return m_pageScalingLayerID.unsafeValue(); }
    void setPageScalingLayerID(WebCore::PlatformLayerIdentifier layerID) { m_pageScalingLayerID = layerID; }

    WebCore::PlatformLayerIdentifier scrolledContentsLayerID() const { return m_scrolledContentsLayerID.unsafeValue(); }
    void setScrolledContentsLayerID(WebCore::PlatformLayerIdentifier layerID) { m_scrolledContentsLayerID = layerID; }
#endif

    uint64_t renderTreeSize() const { return m_renderTreeSize; }
    void setRenderTreeSize(uint64_t renderTreeSize) { m_renderTreeSize = renderTreeSize; }

    double minimumScaleFactor() const { return m_minimumScaleFactor; }
    void setMinimumScaleFactor(double scale) { m_minimumScaleFactor = scale; }

    double maximumScaleFactor() const { return m_maximumScaleFactor; }
    void setMaximumScaleFactor(double scale) { m_maximumScaleFactor = scale; }

    double initialScaleFactor() const { return m_initialScaleFactor; }
    void setInitialScaleFactor(double scale) { m_initialScaleFactor = scale; }

    double viewportMetaTagWidth() const { return m_viewportMetaTagWidth; }
    void setViewportMetaTagWidth(double width) { m_viewportMetaTagWidth = width; }

    bool viewportMetaTagWidthWasExplicit() const { return m_viewportMetaTagWidthWasExplicit; }
    void setViewportMetaTagWidthWasExplicit(bool widthWasExplicit) { m_viewportMetaTagWidthWasExplicit = widthWasExplicit; }

    bool viewportMetaTagCameFromImageDocument() const { return m_viewportMetaTagCameFromImageDocument; }
    void setViewportMetaTagCameFromImageDocument(bool cameFromImageDocument) { m_viewportMetaTagCameFromImageDocument = cameFromImageDocument; }

    bool isInStableState() const { return m_isInStableState; }
    void setIsInStableState(bool isInStableState) { m_isInStableState = isInStableState; }

    bool allowsUserScaling() const { return m_allowsUserScaling; }
    void setAllowsUserScaling(bool allowsUserScaling) { m_allowsUserScaling = allowsUserScaling; }

    bool avoidsUnsafeArea() const { return m_avoidsUnsafeArea; }
    void setAvoidsUnsafeArea(bool avoidsUnsafeArea) { m_avoidsUnsafeArea = avoidsUnsafeArea; }

    TransactionID transactionID() const { return m_transactionID; }
    void setTransactionID(TransactionID transactionID) { m_transactionID = transactionID; }

    ActivityStateChangeID activityStateChangeID() const { return m_activityStateChangeID; }
    void setActivityStateChangeID(ActivityStateChangeID activityStateChangeID) { m_activityStateChangeID = activityStateChangeID; }

    using TransactionCallbackID = IPC::AsyncReplyID;
    const Vector<TransactionCallbackID>& callbackIDs() const { return m_callbackIDs; }
    void setCallbackIDs(Vector<TransactionCallbackID>&& callbackIDs) { m_callbackIDs = WTFMove(callbackIDs); }

    OptionSet<WebCore::LayoutMilestone> newlyReachedPaintingMilestones() const { return m_newlyReachedPaintingMilestones; }
    void setNewlyReachedPaintingMilestones(OptionSet<WebCore::LayoutMilestone> milestones) { m_newlyReachedPaintingMilestones = milestones; }

    bool hasEditorState() const { return !!m_editorState; }
    const EditorState& editorState() const { return m_editorState.value(); }
    void setEditorState(const EditorState& editorState) { m_editorState = editorState; }

#if PLATFORM(IOS_FAMILY)
    std::optional<DynamicViewportSizeUpdateID> dynamicViewportSizeUpdateID() const { return m_dynamicViewportSizeUpdateID; }
    void setDynamicViewportSizeUpdateID(DynamicViewportSizeUpdateID resizeID) { m_dynamicViewportSizeUpdateID = resizeID; }
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    Seconds acceleratedTimelineTimeOrigin() const { return m_acceleratedTimelineTimeOrigin; }
    void setAcceleratedTimelineTimeOrigin(Seconds timeOrigin) { m_acceleratedTimelineTimeOrigin = timeOrigin; }
#endif

private:
    friend struct IPC::ArgumentCoder<RemoteLayerTreeTransaction, void>;

    WebCore::PlatformLayerIdentifier m_rootLayerID;
    ChangedLayers m_changedLayers;

    Markable<WebCore::LayerHostingContextIdentifier> m_remoteContextHostedIdentifier;

    Vector<LayerCreationProperties> m_createdLayers;
    Vector<WebCore::PlatformLayerIdentifier> m_destroyedLayerIDs;
    Vector<WebCore::PlatformLayerIdentifier> m_videoLayerIDsPendingFullscreen;
    Vector<WebCore::PlatformLayerIdentifier> m_layerIDsWithNewlyUnreachableBackingStore;

    Vector<TransactionCallbackID> m_callbackIDs;

    WebCore::IntSize m_contentsSize;
    WebCore::IntPoint m_scrollOrigin;
    WebCore::LayoutSize m_baseLayoutViewportSize;
    WebCore::LayoutPoint m_minStableLayoutViewportOrigin;
    WebCore::LayoutPoint m_maxStableLayoutViewportOrigin;
    WebCore::IntPoint m_scrollPosition;
    WebCore::Color m_themeColor;
    WebCore::Color m_pageExtendedBackgroundColor;
    WebCore::Color m_sampledPageTopColor;

#if PLATFORM(MAC)
    Markable<WebCore::PlatformLayerIdentifier> m_pageScalingLayerID; // Only used for non-delegated scaling.
    Markable<WebCore::PlatformLayerIdentifier> m_scrolledContentsLayerID;
#endif

    double m_pageScaleFactor { 1 };
    double m_minimumScaleFactor { 1 };
    double m_maximumScaleFactor { 1 };
    double m_initialScaleFactor { 1 };
    double m_viewportMetaTagWidth { -1 };
    uint64_t m_renderTreeSize { 0 };
    TransactionID m_transactionID;
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };
    OptionSet<WebCore::LayoutMilestone> m_newlyReachedPaintingMilestones;
    bool m_scaleWasSetByUIProcess { false };
    bool m_allowsUserScaling { false };
    bool m_avoidsUnsafeArea { true };
    bool m_viewportMetaTagWidthWasExplicit { false };
    bool m_viewportMetaTagCameFromImageDocument { false };
    bool m_isInStableState { false };

    std::optional<EditorState> m_editorState;
#if PLATFORM(IOS_FAMILY)
    std::optional<DynamicViewportSizeUpdateID> m_dynamicViewportSizeUpdateID;
#endif
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
    Seconds m_acceleratedTimelineTimeOrigin;
#endif
};

} // namespace WebKit
