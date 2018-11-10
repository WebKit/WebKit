/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DynamicViewportSizeUpdate.h"
#include "EditorState.h"
#include "GenericCallback.h"
#include "PlatformCAAnimationRemote.h"
#include "RemoteLayerBackingStore.h"
#include <WebCore/Color.h>
#include <WebCore/FilterOperations.h>
#include <WebCore/FloatPoint3D.h>
#include <WebCore/FloatSize.h>
#include <WebCore/LayoutMilestone.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerTreeTransaction {
public:
    enum LayerChange {
        NameChanged                     = 1LLU << 1,
        ChildrenChanged                 = 1LLU << 2,
        PositionChanged                 = 1LLU << 3,
        BoundsChanged                   = 1LLU << 4,
        BackgroundColorChanged          = 1LLU << 5,
        AnchorPointChanged              = 1LLU << 6,
        BorderWidthChanged              = 1LLU << 7,
        BorderColorChanged              = 1LLU << 8,
        OpacityChanged                  = 1LLU << 9,
        TransformChanged                = 1LLU << 10,
        SublayerTransformChanged        = 1LLU << 11,
        HiddenChanged                   = 1LLU << 12,
        GeometryFlippedChanged          = 1LLU << 13,
        DoubleSidedChanged              = 1LLU << 14,
        MasksToBoundsChanged            = 1LLU << 15,
        OpaqueChanged                   = 1LLU << 16,
        ContentsHiddenChanged           = 1LLU << 17,
        MaskLayerChanged                = 1LLU << 18,
        ClonedContentsChanged           = 1LLU << 19,
        ContentsRectChanged             = 1LLU << 20,
        ContentsScaleChanged            = 1LLU << 21,
        CornerRadiusChanged             = 1LLU << 22,
        ShapeRoundedRectChanged         = 1LLU << 23,
        ShapePathChanged                = 1LLU << 24,
        MinificationFilterChanged       = 1LLU << 25,
        MagnificationFilterChanged      = 1LLU << 26,
        BlendModeChanged                = 1LLU << 27,
        WindRuleChanged                 = 1LLU << 28,
        SpeedChanged                    = 1LLU << 29,
        TimeOffsetChanged               = 1LLU << 30,
        BackingStoreChanged             = 1LLU << 31,
        BackingStoreAttachmentChanged   = 1LLU << 32,
        FiltersChanged                  = 1LLU << 33,
        AnimationsChanged               = 1LLU << 34,
        EdgeAntialiasingMaskChanged     = 1LLU << 35,
        CustomAppearanceChanged         = 1LLU << 36,
        UserInteractionEnabledChanged   = 1LLU << 37,
    };

    struct LayerCreationProperties {
        LayerCreationProperties();

        void encode(IPC::Encoder&) const;
        static std::optional<LayerCreationProperties> decode(IPC::Decoder&);

        WebCore::GraphicsLayer::PlatformLayerID layerID;
        WebCore::PlatformCALayer::LayerType type;

        uint32_t hostingContextID;
        float hostingDeviceScaleFactor;
    };

    struct LayerProperties {
        LayerProperties();
        LayerProperties(const LayerProperties& other);

        void encode(IPC::Encoder&) const;
        static bool decode(IPC::Decoder&, LayerProperties&);

        void notePropertiesChanged(OptionSet<LayerChange> changeFlags)
        {
            changedProperties.add(changeFlags);
        }

        void resetChangedProperties()
        {
            changedProperties = { };
        }

        OptionSet<LayerChange> changedProperties;

        String name;
        std::unique_ptr<WebCore::TransformationMatrix> transform;
        std::unique_ptr<WebCore::TransformationMatrix> sublayerTransform;
        std::unique_ptr<WebCore::FloatRoundedRect> shapeRoundedRect;

        Vector<WebCore::GraphicsLayer::PlatformLayerID> children;

        Vector<std::pair<String, PlatformCAAnimationRemote::Properties>> addedAnimations;
        HashSet<String> keyPathsOfAnimationsToRemove;

        WebCore::FloatPoint3D position;
        WebCore::FloatPoint3D anchorPoint;
        WebCore::FloatRect bounds;
        WebCore::FloatRect contentsRect;
        std::unique_ptr<RemoteLayerBackingStore> backingStore;
        std::unique_ptr<WebCore::FilterOperations> filters;
        WebCore::Path shapePath;
        WebCore::GraphicsLayer::PlatformLayerID maskLayerID;
        WebCore::GraphicsLayer::PlatformLayerID clonedLayerID;
        double timeOffset;
        float speed;
        float contentsScale;
        float cornerRadius;
        float borderWidth;
        float opacity;
        WebCore::Color backgroundColor;
        WebCore::Color borderColor;
        unsigned edgeAntialiasingMask;
        WebCore::GraphicsLayer::CustomAppearance customAppearance;
        WebCore::PlatformCALayer::FilterType minificationFilter;
        WebCore::PlatformCALayer::FilterType magnificationFilter;
        WebCore::BlendMode blendMode;
        WebCore::WindRule windRule;
        bool hidden;
        bool backingStoreAttached;
        bool geometryFlipped;
        bool doubleSided;
        bool masksToBounds;
        bool opaque;
        bool contentsHidden;
        bool userInteractionEnabled;
    };

    explicit RemoteLayerTreeTransaction();
    ~RemoteLayerTreeTransaction();

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RemoteLayerTreeTransaction&);

    WebCore::GraphicsLayer::PlatformLayerID rootLayerID() const { return m_rootLayerID; }
    void setRootLayerID(WebCore::GraphicsLayer::PlatformLayerID);
    void layerPropertiesChanged(PlatformCALayerRemote&);
    void setCreatedLayers(Vector<LayerCreationProperties>);
    void setDestroyedLayerIDs(Vector<WebCore::GraphicsLayer::PlatformLayerID>);
    void setLayerIDsWithNewlyUnreachableBackingStore(Vector<WebCore::GraphicsLayer::PlatformLayerID>);

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
    void dump() const;
#endif

    typedef HashMap<WebCore::GraphicsLayer::PlatformLayerID, std::unique_ptr<LayerProperties>> LayerPropertiesMap;

    Vector<LayerCreationProperties> createdLayers() const { return m_createdLayers; }
    Vector<WebCore::GraphicsLayer::PlatformLayerID> destroyedLayers() const { return m_destroyedLayerIDs; }
    Vector<WebCore::GraphicsLayer::PlatformLayerID> layerIDsWithNewlyUnreachableBackingStore() const { return m_layerIDsWithNewlyUnreachableBackingStore; }

    Vector<RefPtr<PlatformCALayerRemote>>& changedLayers() { return m_changedLayers; }

    const LayerPropertiesMap& changedLayerProperties() const { return m_changedLayerProperties; }
    LayerPropertiesMap& changedLayerProperties() { return m_changedLayerProperties; }

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
    
    WebCore::Color pageExtendedBackgroundColor() const { return m_pageExtendedBackgroundColor; }
    void setPageExtendedBackgroundColor(WebCore::Color color) { m_pageExtendedBackgroundColor = color; }

    WebCore::IntPoint scrollPosition() const { return m_scrollPosition; }
    void setScrollPosition(WebCore::IntPoint p) { m_scrollPosition = p; }

    double pageScaleFactor() const { return m_pageScaleFactor; }
    void setPageScaleFactor(double pageScaleFactor) { m_pageScaleFactor = pageScaleFactor; }

    bool scaleWasSetByUIProcess() const { return m_scaleWasSetByUIProcess; }
    void setScaleWasSetByUIProcess(bool scaleWasSetByUIProcess) { m_scaleWasSetByUIProcess = scaleWasSetByUIProcess; }
    
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

    uint64_t transactionID() const { return m_transactionID; }
    void setTransactionID(uint64_t transactionID) { m_transactionID = transactionID; }

    ActivityStateChangeID activityStateChangeID() const { return m_activityStateChangeID; }
    void setActivityStateChangeID(ActivityStateChangeID activityStateChangeID) { m_activityStateChangeID = activityStateChangeID; }

    typedef CallbackID TransactionCallbackID;
    const Vector<TransactionCallbackID>& callbackIDs() const { return m_callbackIDs; }
    void setCallbackIDs(Vector<TransactionCallbackID>&& callbackIDs) { m_callbackIDs = WTFMove(callbackIDs); }

    OptionSet<WebCore::LayoutMilestone> newlyReachedLayoutMilestones() const { return m_newlyReachedLayoutMilestones; }
    void setNewlyReachedLayoutMilestones(OptionSet<WebCore::LayoutMilestone> milestones) { m_newlyReachedLayoutMilestones = milestones; }

    bool hasEditorState() const { return !!m_editorState; }
    const EditorState& editorState() const { return m_editorState.value(); }
    void setEditorState(const EditorState& editorState) { m_editorState = editorState; }

    std::optional<DynamicViewportSizeUpdateID> dynamicViewportSizeUpdateID() const { return m_dynamicViewportSizeUpdateID; }
    void setDynamicViewportSizeUpdateID(DynamicViewportSizeUpdateID resizeID) { m_dynamicViewportSizeUpdateID = resizeID; }
    
private:
    WebCore::GraphicsLayer::PlatformLayerID m_rootLayerID;
    Vector<RefPtr<PlatformCALayerRemote>> m_changedLayers; // Only used in the Web process.
    LayerPropertiesMap m_changedLayerProperties; // Only used in the UI process.

    Vector<LayerCreationProperties> m_createdLayers;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> m_destroyedLayerIDs;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> m_videoLayerIDsPendingFullscreen;
    Vector<WebCore::GraphicsLayer::PlatformLayerID> m_layerIDsWithNewlyUnreachableBackingStore;

    Vector<TransactionCallbackID> m_callbackIDs;

    WebCore::IntSize m_contentsSize;
    WebCore::IntPoint m_scrollOrigin;
    WebCore::LayoutSize m_baseLayoutViewportSize;
    WebCore::LayoutPoint m_minStableLayoutViewportOrigin;
    WebCore::LayoutPoint m_maxStableLayoutViewportOrigin;
    WebCore::IntPoint m_scrollPosition;
    WebCore::Color m_pageExtendedBackgroundColor;
    double m_pageScaleFactor { 1 };
    double m_minimumScaleFactor { 1 };
    double m_maximumScaleFactor { 1 };
    double m_initialScaleFactor { 1 };
    double m_viewportMetaTagWidth { -1 };
    uint64_t m_renderTreeSize { 0 };
    uint64_t m_transactionID { 0 };
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };
    OptionSet<WebCore::LayoutMilestone> m_newlyReachedLayoutMilestones;
    bool m_scaleWasSetByUIProcess { false };
    bool m_allowsUserScaling { false };
    bool m_avoidsUnsafeArea { true };
    bool m_viewportMetaTagWidthWasExplicit { false };
    bool m_viewportMetaTagCameFromImageDocument { false };
    bool m_isInStableState { false };

    std::optional<EditorState> m_editorState;
    std::optional<DynamicViewportSizeUpdateID> m_dynamicViewportSizeUpdateID;
};

} // namespace WebKit
