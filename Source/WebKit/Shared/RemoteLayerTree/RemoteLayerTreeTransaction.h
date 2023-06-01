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
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatSize.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/Model.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#include "DynamicViewportSizeUpdate.h"
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include <WebCore/AcceleratedEffect.h>
#include <WebCore/AcceleratedEffectValues.h>
#endif

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

enum class LayerChange : uint64_t {
    NameChanged                         = 1LLU << 1,
    ChildrenChanged                     = 1LLU << 2,
    PositionChanged                     = 1LLU << 3,
    BoundsChanged                       = 1LLU << 4,
    BackgroundColorChanged              = 1LLU << 5,
    AnchorPointChanged                  = 1LLU << 6,
    BorderWidthChanged                  = 1LLU << 7,
    BorderColorChanged                  = 1LLU << 8,
    OpacityChanged                      = 1LLU << 9,
    TransformChanged                    = 1LLU << 10,
    SublayerTransformChanged            = 1LLU << 11,
    HiddenChanged                       = 1LLU << 12,
    GeometryFlippedChanged              = 1LLU << 13,
    DoubleSidedChanged                  = 1LLU << 14,
    MasksToBoundsChanged                = 1LLU << 15,
    OpaqueChanged                       = 1LLU << 16,
    ContentsHiddenChanged               = 1LLU << 17,
    MaskLayerChanged                    = 1LLU << 18,
    ClonedContentsChanged               = 1LLU << 19,
    ContentsRectChanged                 = 1LLU << 20,
    ContentsScaleChanged                = 1LLU << 21,
    CornerRadiusChanged                 = 1LLU << 22,
    ShapeRoundedRectChanged             = 1LLU << 23,
    ShapePathChanged                    = 1LLU << 24,
    MinificationFilterChanged           = 1LLU << 25,
    MagnificationFilterChanged          = 1LLU << 26,
    BlendModeChanged                    = 1LLU << 27,
    WindRuleChanged                     = 1LLU << 28,
    SpeedChanged                        = 1LLU << 29,
    TimeOffsetChanged                   = 1LLU << 30,
    BackingStoreChanged                 = 1LLU << 31,
    BackingStoreAttachmentChanged       = 1LLU << 32,
    FiltersChanged                      = 1LLU << 33,
    AnimationsChanged                   = 1LLU << 34,
    AntialiasesEdgesChanged             = 1LLU << 35,
    CustomAppearanceChanged             = 1LLU << 36,
    UserInteractionEnabledChanged       = 1LLU << 37,
    EventRegionChanged                  = 1LLU << 38,
    ScrollingNodeIDChanged              = 1LLU << 39,
#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    SeparatedChanged                    = 1LLU << 40,
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
    SeparatedPortalChanged              = 1LLU << 41,
    DescendentOfSeparatedPortalChanged  = 1LLU << 42,
#endif
#endif
    VideoGravityChanged                 = 1LLU << 44,
};

class RemoteLayerTreeTransaction {
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct LayerCreationProperties {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        LayerCreationProperties();

        void encode(IPC::Encoder&) const;
        static std::optional<LayerCreationProperties> decode(IPC::Decoder&);

        WebCore::PlatformLayerIdentifier layerID;
        WebCore::PlatformCALayer::LayerType type;
        std::optional<PlaybackSessionContextIdentifier> playerIdentifier;
        std::optional<WebCore::FloatSize> initialSize;
        std::optional<WebCore::FloatSize> naturalSize;

        uint32_t hostingContextID;
        float hostingDeviceScaleFactor;
        bool preservesFlip;

        // FIXME: This could be a variant<CustomData, HostData, ModelData, NoData>.
        Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier;

#if ENABLE(MODEL_ELEMENT)
        RefPtr<WebCore::Model> model;
#endif
    };

    struct LayerProperties {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        LayerProperties();
        LayerProperties(const LayerProperties& other);

        void encode(IPC::Encoder&) const;
        static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, LayerProperties&);

        void notePropertiesChanged(OptionSet<LayerChange> changeFlags)
        {
            changedProperties.add(changeFlags);
            everChangedProperties.add(changeFlags);
        }

        void resetChangedProperties()
        {
            changedProperties = { };
        }

        OptionSet<LayerChange> changedProperties;
        OptionSet<LayerChange> everChangedProperties;

        String name;
        std::unique_ptr<WebCore::TransformationMatrix> transform;
        std::unique_ptr<WebCore::TransformationMatrix> sublayerTransform;
        std::unique_ptr<WebCore::FloatRoundedRect> shapeRoundedRect;

        Vector<WebCore::PlatformLayerIdentifier> children;

        Vector<std::pair<String, PlatformCAAnimationRemote::Properties>> addedAnimations;
        HashSet<String> keysOfAnimationsToRemove;
#if ENABLE(THREADED_ANIMATION_RESOLUTION)
        Vector<Ref<WebCore::AcceleratedEffect>> effects;
        WebCore::AcceleratedEffectValues baseValues;
#endif

        WebCore::FloatPoint3D position;
        WebCore::FloatPoint3D anchorPoint { 0.5, 0.5, 0 };
        WebCore::FloatRect bounds;
        WebCore::FloatRect contentsRect { 0, 0, 1, 1 };
        // Used in the WebContent process.
        std::unique_ptr<RemoteLayerBackingStore> backingStore;
        // Used in the UI process.
        std::unique_ptr<RemoteLayerBackingStoreProperties> backingStoreProperties;
        std::unique_ptr<WebCore::FilterOperations> filters;
        WebCore::Path shapePath;
        Markable<WebCore::PlatformLayerIdentifier> maskLayerID;
        Markable<WebCore::PlatformLayerIdentifier> clonedLayerID;
#if ENABLE(SCROLLING_THREAD)
        WebCore::ScrollingNodeID scrollingNodeID { 0 };
#endif
        double timeOffset { 0 };
        float speed { 1 };
        float contentsScale { 1 };
        float cornerRadius { 0 };
        float borderWidth { 0 };
        float opacity { 1 };
        WebCore::Color backgroundColor { WebCore::Color::transparentBlack };
        WebCore::Color borderColor { WebCore::Color::black };
        WebCore::GraphicsLayer::CustomAppearance customAppearance { WebCore::GraphicsLayer::CustomAppearance::None };
        WebCore::PlatformCALayer::FilterType minificationFilter { WebCore::PlatformCALayer::FilterType::Linear };
        WebCore::PlatformCALayer::FilterType magnificationFilter { WebCore::PlatformCALayer::FilterType::Linear };
        WebCore::BlendMode blendMode { WebCore::BlendMode::Normal };
        WebCore::WindRule windRule { WebCore::WindRule::NonZero };
        WebCore::MediaPlayerVideoGravity videoGravity { WebCore::MediaPlayerVideoGravity::ResizeAspect };
        bool antialiasesEdges { true };
        bool hidden { false };
        bool backingStoreAttached { true };
        bool geometryFlipped { false };
        bool doubleSided { false };
        bool masksToBounds { false };
        bool opaque { false };
        bool contentsHidden { false };
        bool userInteractionEnabled { true };
        WebCore::EventRegion eventRegion;

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
        bool isSeparated { false };
#if HAVE(CORE_ANIMATION_SEPARATED_PORTALS)
        bool isSeparatedPortal { false };
        bool isDescendentOfSeparatedPortal { false };
#endif
#endif
    };

    explicit RemoteLayerTreeTransaction();
    ~RemoteLayerTreeTransaction();
    RemoteLayerTreeTransaction(RemoteLayerTreeTransaction&&);
    RemoteLayerTreeTransaction& operator=(RemoteLayerTreeTransaction&&);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RemoteLayerTreeTransaction&);

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

    typedef HashMap<WebCore::PlatformLayerIdentifier, std::unique_ptr<LayerProperties>> LayerPropertiesMap;
    
    bool hasAnyLayerChanges() const
    {
        return m_changedLayerProperties.size() || m_changedLayers.size() || m_createdLayers.size() || m_destroyedLayerIDs.size() || m_layerIDsWithNewlyUnreachableBackingStore.size();
    }

    const Vector<LayerCreationProperties>& createdLayers() const { return m_createdLayers; }
    const Vector<WebCore::PlatformLayerIdentifier>& destroyedLayers() const { return m_destroyedLayerIDs; }
    const Vector<WebCore::PlatformLayerIdentifier>& layerIDsWithNewlyUnreachableBackingStore() const { return m_layerIDsWithNewlyUnreachableBackingStore; }

    HashSet<RefPtr<PlatformCALayerRemote>>& changedLayers() { return m_changedLayers; }

    const LayerPropertiesMap& changedLayerProperties() const { return m_changedLayerProperties; }
    LayerPropertiesMap& changedLayerProperties() { return m_changedLayerProperties; }

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
    WebCore::PlatformLayerIdentifier pageScalingLayerID() const { return m_pageScalingLayerID.value(); }
    void setPageScalingLayerID(WebCore::PlatformLayerIdentifier layerID) { m_pageScalingLayerID = layerID; }

    WebCore::PlatformLayerIdentifier scrolledContentsLayerID() const { return m_scrolledContentsLayerID.value(); }
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

private:
    WebCore::PlatformLayerIdentifier m_rootLayerID;
    HashSet<RefPtr<PlatformCALayerRemote>> m_changedLayers; // Only used in the Web process.
    LayerPropertiesMap m_changedLayerProperties; // Only used in the UI process.

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
};

} // namespace WebKit
