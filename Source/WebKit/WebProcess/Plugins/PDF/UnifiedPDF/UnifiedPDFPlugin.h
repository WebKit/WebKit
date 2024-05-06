/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(UNIFIED_PDF)

#include "PDFDocumentLayout.h"
#include "PDFPluginBase.h"
#include <WebCore/ElementIdentifier.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Page.h>
#include <WebCore/Timer.h>
#include <wtf/OptionSet.h>

OBJC_CLASS PDFAction;
OBJC_CLASS PDFDestination;
OBJC_CLASS PDFPage;
OBJC_CLASS WKPDFFormMutationObserver;

namespace WebCore {
class FrameView;
class KeyboardScrollingAnimator;
class PageOverlay;

enum class DelegatedScrollingMode : uint8_t;

struct DataDetectorElementInfo;
}

namespace WebKit {

class AsyncPDFRenderer;
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
class PDFDataDetectorOverlayController;
#endif
class PDFPluginPasswordField;
class PDFPluginPasswordForm;
class WebFrame;
class WebMouseEvent;
struct PDFContextMenu;
struct PDFContextMenuItem;
struct PDFPageCoverage;

enum class WebEventType : uint8_t;
enum class WebMouseEventButton : int8_t;
enum class WebEventModifier : uint8_t;

enum class RepaintRequirement : uint8_t {
    PDFContent      = 1 << 0,
    Selection       = 1 << 1,
    HoverOverlay    = 1 << 2
};

class AnnotationTrackingState {
public:
    OptionSet<RepaintRequirement> startAnnotationTracking(RetainPtr<PDFAnnotation>&&, WebEventType, WebMouseEventButton);
    OptionSet<RepaintRequirement> finishAnnotationTracking(PDFAnnotation* annotationUnderMouse, WebEventType, WebMouseEventButton);

    PDFAnnotation *trackedAnnotation() const { return m_trackedAnnotation.get(); }
    bool isBeingHovered() const;

private:
    void resetAnnotationTrackingState();

    RetainPtr<PDFAnnotation> m_trackedAnnotation;
    bool m_isBeingHovered { false };
};

enum class AnnotationSearchDirection : bool {
    Forward,
    Backward
};

class UnifiedPDFPlugin final : public PDFPluginBase, public WebCore::GraphicsLayerClient {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(UnifiedPDFPlugin);

    friend class AsyncPDFRenderer;
public:
    static Ref<UnifiedPDFPlugin> create(WebCore::HTMLPlugInElement&);
    virtual ~UnifiedPDFPlugin();

    enum class PDFElementType : uint16_t {
        Page       = 1 << 0,
        Text       = 1 << 1,
        Annotation = 1 << 2,
        Link       = 1 << 3,
        Control    = 1 << 4,
        TextField  = 1 << 5,
        Icon       = 1 << 6,
        Popup      = 1 << 7,
        Image      = 1 << 8,
    };
    using PDFElementTypes = OptionSet<PDFElementType>;

    WebCore::FrameView* mainFrameView() const;

    CGRect pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>&) const final;
    void setActiveAnnotation(RetainPtr<PDFAnnotation>&&) final;
    void focusNextAnnotation() final;
    void focusPreviousAnnotation() final;
#if PLATFORM(MAC)
    RetainPtr<PDFAnnotation> nextTextAnnotation(AnnotationSearchDirection) const;
    void handlePDFActionForAnnotation(PDFAnnotation *, unsigned currentPageIndex);
#endif
    enum class IsAnnotationCommit : bool { No, Yes };
    static OptionSet<RepaintRequirement> repaintRequirementsForAnnotation(PDFAnnotation *, IsAnnotationCommit = IsAnnotationCommit::No);
    void repaintAnnotationsForFormField(NSString *fieldName);

    Vector<WebCore::FloatRect> annotationRectsForTesting() const final;

    void attemptToUnlockPDF(const String& password) final;
    void windowActivityDidChange() final;

    void didSameDocumentNavigationForFrame(WebFrame&) final;

    float documentFittingScale() const { return m_documentLayout.scale(); }

#if PLATFORM(MAC)
    WebCore::FloatRect convertFromPDFPageToScreenForAccessibility(const WebCore::FloatRect&, PDFDocumentLayout::PageIndex) const;
    void accessibilityScrollToPage(PDFDocumentLayout::PageIndex);
#endif

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    void installDataDetectorOverlay(WebCore::PageOverlay&);
    void uninstallDataDetectorOverlay(WebCore::PageOverlay&);

    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&);
#endif

    void scheduleRenderingUpdate(OptionSet<WebCore::RenderingUpdateStep> = WebCore::RenderingUpdateStep::LayerFlush);
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayerClient&);
    float deviceScaleFactor() const override;

    WebCore::FloatRect rectForSelectionInMainFrameContentsSpace(PDFSelection *) const;

    /*
        Unified PDF Plugin coordinate spaces, in depth order:

        - "root view": same as the rest of WebKit.

        - "plugin": the space of the plugin element (origin at the top left,
            ignoring all internal transforms).

        - "contents": the space of the contents layer, with scrolling subtracted
            out and page scale multiplied in; the painting space.

        - "document": the space that the PDF pages are laid down in, with
            PDFDocumentLayout's width-fitting scale divided out; includes margins.

        - "page": the space of each actual PDFPage, as used by PDFKit; origin at
            the bottom left of the crop box; page rotation multiplied in.
    */

    enum class CoordinateSpace : uint8_t {
        PDFPage,
        PDFDocumentLayout,
        Contents, // aka "ScaledDocument" aka "Painting"
        ScrolledContents,
        Plugin
    };

    Vector<WebCore::FloatRect> boundsForSelection(const PDFSelection *, CoordinateSpace inSpace) const;

    RetainPtr<PDFPage> pageAtIndex(PDFDocumentLayout::PageIndex) const;

    void setPDFDisplayModeForTesting(const String&) final;

private:
    explicit UnifiedPDFPlugin(WebCore::HTMLPlugInElement&);
    bool isUnifiedPDFPlugin() const override { return true; }

    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::GraphicsLayer; }
    WebCore::GraphicsLayer* graphicsLayer() const override;

    void teardown() override;

    void installPDFDocument() override;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    void enableDataDetection();
    void didInvalidateDataDetectorHighlightOverlayRects();

    PDFDataDetectorOverlayController& dataDetectorOverlayController() { return *m_dataDetectorOverlayController; }
#endif

    double scaleForActualSize() const;
    double initialScale() const;
    double scaleForFitToView() const;

    /*
        Unified PDF Plugin scales, in depth order:

        - "device": same as the rest of WebKit. CSS-to-screen pixel ratio.

        - "page": the scale of the WebPage.

        - "scale factor": the user's chosen scale for the PDF contents (by zoom buttons, pinch-zoom, etc.).
            for main frame plugins, this is synced with the page scale
            for embedded plugins, this is on top of the page scale

        - "document layout scale": the scale between contents and document space, to fit the pages in the scroll view's contents

        Convenience names:

        - "contentScaleFactor": the scale between the plugin and document space (scaleFactor * document layout scale)
    */
    double scaleFactor() const override;
    double contentScaleFactor() const final;

    // Scale normalization is used to map the internal "scale factor" to the exposed scaleFactor()/setPageScaleFactor()
    // so that scale factor 1 shows at "Actual Size".
    void computeNormalizationFactor();
    double fromNormalizedScaleFactor(double) const;
    double toNormalizedScaleFactor(double) const;

    void didBeginMagnificationGesture() override;
    void didEndMagnificationGesture() override;
    void setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin) final;
    void setScaleFactor(double scale, std::optional<WebCore::IntPoint> origin = std::nullopt);

    WebCore::IntSize documentSize() const;
    WebCore::IntSize contentsSize() const override;
    unsigned firstPageHeight() const override;
    unsigned heightForPageAtIndex(PDFDocumentLayout::PageIndex) const;
    WebCore::FloatRect layoutBoundsForPageAtIndex(PDFDocumentLayout::PageIndex) const;

    enum class AdjustScaleAfterLayout : bool {
        No,
        Yes
    };
    void updateLayout(AdjustScaleAfterLayout = AdjustScaleAfterLayout::No);

    WebCore::IntRect availableContentsRect() const;

    WebCore::DelegatedScrollingMode scrollingMode() const;
    bool isFullMainFramePlugin() const;

    OptionSet<WebCore::TiledBackingScrollability> computeScrollability() const;

    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;
    void updateScrollbars() override;
    void willAttachScrollingNode() final;
    void didAttachScrollingNode() final;

    bool geometryDidChange(const WebCore::IntSize&, const WebCore::AffineTransform&) override;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const override;

    NSData *liveData() const override;

    bool wantsWheelEvents() const override { return false; }
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override { return false; }
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;

    // Editing commands
    bool handleEditingCommand(const String& commandName, const String& argument) override;
    bool isEditingCommandEnabled(const String& commandName) override;
    bool takeFindStringFromSelection();
    bool forwardEditingCommandToEditor(const String& commandName, const String& argument) const;
    void selectAll();
    [[maybe_unused]] bool performCopyEditingOperation() const;
    void performCopyLinkOperation(const WebCore::IntPoint& contextMenuEventRootViewPoint) const;

#if PLATFORM(MAC)
    bool handleKeyboardCommand(const WebKeyboardEvent&);
    bool handleKeyboardEventForDiscreteDisplayMode(const WebKeyboardEvent&);
    CheckedPtr<WebCore::KeyboardScrollingAnimator> checkedKeyboardScrollingAnimator() const;
#endif

    void animatedScrollDidEnd() final;

    void setDisplayModeAndUpdateLayout(PDFDocumentLayout::DisplayMode);

    // Context Menu
#if ENABLE(CONTEXT_MENUS)
    enum class ContextMenuItemTag : int8_t {
        Invalid = -1,
        AutoSize,
        WebSearch,
        DictionaryLookup,
        Copy,
        CopyLink,
        NextPage,
        OpenWithPreview,
        PreviousPage,
        SinglePage,
        SinglePageContinuous,
        TwoPages,
        TwoPagesContinuous,
        ZoomIn,
        ZoomOut,
        ActualSize,
        Unknown,
    };

    std::optional<PDFContextMenu> createContextMenu(const WebMouseEvent&) const;
    PDFContextMenuItem contextMenuItem(ContextMenuItemTag, bool hasAction = true) const;
    String titleForContextMenuItemTag(ContextMenuItemTag) const;
    bool isDisplayModeContextMenuItemTag(ContextMenuItemTag) const;
    PDFContextMenuItem separatorContextMenuItem() const;
    Vector<PDFContextMenuItem> selectionContextMenuItems(const WebCore::IntPoint& contextMenuEventRootViewPoint) const;
    Vector<PDFContextMenuItem> displayModeContextMenuItems() const;
    Vector<PDFContextMenuItem> scaleContextMenuItems() const;
    Vector<PDFContextMenuItem> navigationContextMenuItemsForPageAtIndex(PDFDocumentLayout::PageIndex) const;
    ContextMenuItemTag toContextMenuItemTag(int tagValue) const;
    void performContextMenuAction(ContextMenuItemTag, const WebCore::IntPoint& contextMenuEventRootViewPoint);

    ContextMenuItemTag contextMenuItemTagFromDisplayMode(const PDFDocumentLayout::DisplayMode&) const;
    PDFDocumentLayout::DisplayMode displayModeFromContextMenuItemTag(const ContextMenuItemTag&) const;
#endif

    // Autoscroll
    // FIXME: Refactor and hook into WebCore::AutoscrollController instead of manually implementing these.
    void beginAutoscroll();
    void autoscrollTimerFired();
    void continueAutoscroll();
    void stopAutoscroll();
    void scrollWithDelta(const WebCore::IntSize&);

    // Selections
    enum class SelectionGranularity : uint8_t {
        Character,
        Word,
        Line,
    };

    enum class ActiveStateChangeReason : bool {
        SetCurrentSelection,
        WindowActivityChanged,
    };

    enum class IsDraggingSelection : bool { No, Yes };

    SelectionGranularity selectionGranularityForMouseEvent(const WebMouseEvent&) const;
    void beginTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::FloatPoint& pagePoint, const WebMouseEvent&);
    void extendCurrentSelectionIfNeeded();
    void updateCurrentSelectionForContextMenuEventIfNeeded();
    void continueTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::FloatPoint& pagePoint, IsDraggingSelection);
    void freezeCursorDuringSelectionDragIfNeeded(IsDraggingSelection);
    void unfreezeCursorAfterSelectionDragIfNeeded();
    void stopTrackingSelection();
    void setCurrentSelection(RetainPtr<PDFSelection>&&);
    RetainPtr<PDFSelection> protectedCurrentSelection() const;
    void repaintOnSelectionActiveStateChangeIfNeeded(ActiveStateChangeReason, const Vector<WebCore::FloatRect>& additionalDocumentRectsToRepaint = { });

    String selectionString() const override;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const override;

    // Find in PDF
    enum class HideFindIndicator : bool { No, Yes };
    unsigned countFindMatches(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String&, WebCore::FindOptions, unsigned maxMatchCount) override;
    Vector<WebCore::FloatRect> rectsForTextMatchesInRect(const WebCore::IntRect&) const final;
    bool drawsFindOverlay() const final { return false; }
    void collectFindMatchRects(const String&, WebCore::FindOptions);
    void updateFindOverlay(HideFindIndicator = HideFindIndicator::No);

    RefPtr<WebCore::TextIndicator> textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition) final;
    RefPtr<WebCore::TextIndicator> textIndicatorForSelection(PDFSelection *, OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition);
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    std::optional<WebCore::FloatRect> selectionBoundsForFirstPageInDocumentSpace(const RetainPtr<PDFSelection>&) const;
    bool showDefinitionForSelection(PDFSelection *);
    std::pair<String, RetainPtr<PDFSelection>> textForImmediateActionHitTestAtPoint(const WebCore::FloatPoint&, WebHitTestResultData&) override;
    WebCore::DictionaryPopupInfo dictionaryPopupInfoForSelection(PDFSelection *, WebCore::TextIndicatorPresentationTransition) override;

    id accessibilityHitTest(const WebCore::IntPoint&) const override;
    id accessibilityObject() const override;
#if PLATFORM(MAC)
    id accessibilityHitTestIntPoint(const WebCore::IntPoint&) const;
#endif

    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) override;

    // GraphicsLayerClient
    void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect&, OptionSet<WebCore::GraphicsLayerPaintBehavior>) override;
    float pageScaleFactor() const override;
    bool layerNeedsPlatformContext(const WebCore::GraphicsLayer*) const override;
    void tiledBackingUsageChanged(const WebCore::GraphicsLayer*, bool /*usingTiledBacking*/) override;
    std::optional<float> customContentsScale(const WebCore::GraphicsLayer*) const override;

    // Package up the data needed to paint a set of pages for the given clip, for use by UnifiedPDFPlugin::paintPDFContent and async rendering.
    PDFPageCoverage pageCoverageForRect(const WebCore::FloatRect& clipRect) const;

    enum class PaintingBehavior : bool { All, PageContentsOnly };
    enum class AllowsAsyncRendering : bool { No, Yes };
    void paintPDFContent(WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, PaintingBehavior = PaintingBehavior::All, AllowsAsyncRendering = AllowsAsyncRendering::No);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    void paintPDFSelection(WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect);
#endif
    bool canPaintSelectionIntoOwnedLayer() const;

    void ensureLayers();
    void updatePageBackgroundLayers();
    void updateLayerHierarchy();
    void updateLayerPositions();

    void didChangeScrollOffset() override;
    void didChangeIsInWindow();

    void didChangeSettings() override;

    void createScrollbarsController() override;

    bool usesAsyncScrolling() const final { return true; }
    WebCore::ScrollingNodeID scrollingNodeID() const final;

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;

    WebCore::GraphicsLayer* layerForHorizontalScrollbar() const override;
    WebCore::GraphicsLayer* layerForVerticalScrollbar() const override;
    WebCore::GraphicsLayer* layerForScrollCorner() const override;

    void updateScrollingExtents();

    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer);
    void positionOverflowControlsLayers();

    WebCore::ScrollingCoordinator* scrollingCoordinator();
    void createScrollingNodeIfNecessary();

    void revealRectInContentsSpace(WebCore::FloatRect);
    void scrollToPointInContentsSpace(WebCore::FloatPoint);
    void scrollToPDFDestination(PDFDestination *);
    void scrollToPointInPage(WebCore::FloatPoint pointInPDFPageSpace, PDFDocumentLayout::PageIndex);
    void scrollToPage(PDFDocumentLayout::PageIndex);
    void scrollToFragmentIfNeeded();

    // ScrollableArea
    bool requestScrollToPosition(const WebCore::ScrollPosition&, const WebCore::ScrollPositionChangeOptions& = WebCore::ScrollPositionChangeOptions::createProgrammatic()) override;
    bool requestStartKeyboardScrollAnimation(const WebCore::KeyboardScroll& scrollData) override;
    bool requestStopKeyboardScrollAnimation(bool immediate) override;
    void updateSnapOffsets() override;

    bool shouldDisplayPage(PDFDocumentLayout::PageIndex);
    void populateScrollSnapIdentifiers();
    PDFDocumentLayout::PageIndex pageForScrollSnapIdentifier(WebCore::ElementIdentifier) const;
    void determineCurrentlySnappedPage();

    WebCore::FloatSize centeringOffset() const;

    struct ScrollAnchoringInfo {
        PDFDocumentLayout::PageIndex pageIndex { 0 };
        WebCore::FloatPoint pagePoint;
    };

    std::optional<ScrollAnchoringInfo> scrollAnchoringForCurrentScrollPosition(bool preserveScrollPosition) const;
    void restoreScrollPositionWithInfo(const ScrollAnchoringInfo&);

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
    void resetZoom();
#endif

    std::optional<PDFDocumentLayout::PageIndex> pageIndexWithHoveredAnnotation() const;
    void paintHoveredAnnotationOnPage(PDFDocumentLayout::PageIndex, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect);

    WebCore::FloatRect documentRectForAnnotation(PDFAnnotation *) const;

    void followLinkAnnotation(PDFAnnotation *);

    void startTrackingAnnotation(RetainPtr<PDFAnnotation>&&, WebEventType, WebMouseEventButton);
    void updateTrackedAnnotation(PDFAnnotation *annotationUnderMouse);
    void finishTrackingAnnotation(PDFAnnotation *annotationUnderMouse, WebEventType, WebMouseEventButton, OptionSet<RepaintRequirement> = { });

    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(GraphicsLayerClient&, WebCore::GraphicsLayer::Type);
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String& name, WebCore::GraphicsLayer::Type);

    void setNeedsRepaintInDocumentRect(OptionSet<RepaintRequirement>, const WebCore::FloatRect&);
    void setNeedsRepaintInDocumentRects(OptionSet<RepaintRequirement>, const Vector<WebCore::FloatRect>&);

    // "Up" is inside-out.
    template <typename T>
    T convertUp(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex = { }) const;

    // "Down" is outside-in.
    template <typename T>
    T convertDown(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> = { }) const;

    std::optional<PDFDocumentLayout::PageIndex> pageIndexForDocumentPoint(const WebCore::FloatPoint&) const;
    PDFDocumentLayout::PageIndex indexForCurrentPageInView() const;

    RetainPtr<PDFAnnotation> annotationForRootViewPoint(const WebCore::IntPoint&) const;

    PDFElementTypes pdfElementTypesForPluginPoint(const WebCore::IntPoint&) const;

    bool isTaggedPDF() const;

    bool shouldShowDebugIndicators() const;

    void paintBackgroundLayerForPage(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect&, PDFDocumentLayout::PageIndex);
    float scaleForPagePreviews() const;
    void didGeneratePreviewForPage(PDFDocumentLayout::PageIndex);
    WebCore::GraphicsLayer* backgroundLayerForPage(PDFDocumentLayout::PageIndex) const;
    std::optional<PDFDocumentLayout::PageIndex> pageIndexForPageBackgroundLayer(const WebCore::GraphicsLayer*) const;

#if PLATFORM(MAC)
    void createPasswordEntryForm();

    bool snapToNearbyPageExtentForKeyboardScrolling(WebCore::ScrollDirection);
#endif

    bool isInDiscreteDisplayMode() const;
    bool isShowingTwoPages() const;

    WebCore::FloatRect pageBoundsInContentsSpace(PDFDocumentLayout::PageIndex) const;

    Ref<AsyncPDFRenderer> asyncRenderer();
    RefPtr<AsyncPDFRenderer> asyncRendererIfExists() const;

    PDFDocumentLayout m_documentLayout;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrollContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrolledContentsLayer;
    RefPtr<WebCore::GraphicsLayer> m_pageBackgroundsContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_contentsLayer;
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    RefPtr<WebCore::GraphicsLayer> m_selectionLayer;
#endif

    RefPtr<WebCore::GraphicsLayer> m_overflowControlsContainer;
    RefPtr<WebCore::GraphicsLayer> m_layerForHorizontalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForVerticalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForScrollCorner;

    HashMap<RefPtr<WebCore::GraphicsLayer>, PDFDocumentLayout::PageIndex> m_pageBackgroundLayers;

    WebCore::ScrollingNodeID m_scrollingNodeID;

    double m_scaleFactor { 1 };
    double m_scaleNormalizationFactor { 1 };

    std::optional<WebCore::IntPoint> m_magnificationOriginInContentCoordinates;
    std::optional<WebCore::IntPoint> m_magnificationOriginInPluginCoordinates;

    bool m_inMagnificationGesture { false };
    bool m_didAttachScrollingTreeNode { false };
    bool m_didScrollToFragment { false };
    bool m_didLayoutWithValidDocument { false };

    ShouldUpdateAutoSizeScale m_shouldUpdateAutoSizeScale { ShouldUpdateAutoSizeScale::Yes };

    AnnotationTrackingState m_annotationTrackingState;

    struct SelectionTrackingData {
        bool isActivelyTrackingSelection { false };
        bool shouldExtendCurrentSelection { false };
        bool shouldMakeMarqueeSelection { false };
        bool cursorIsFrozenForSelectionDrag { false };
        SelectionGranularity granularity { SelectionGranularity::Character };
        PDFDocumentLayout::PageIndex startPageIndex;
        WebCore::FloatPoint startPagePoint;
        RetainPtr<PDFSelection> selectionToExtendWith;
        WebCore::FloatRect marqueeSelectionRect;
    };
    SelectionTrackingData m_selectionTrackingData;
    RetainPtr<PDFSelection> m_currentSelection;

    bool m_inActiveAutoscroll { false };
    WebCore::Timer m_autoscrollTimer { *this, &UnifiedPDFPlugin::autoscrollTimerFired };

    RetainPtr<WKPDFFormMutationObserver> m_pdfMutationObserver;

#if PLATFORM(MAC)
    RefPtr<PDFPluginPasswordField> m_passwordField;
    RefPtr<PDFPluginPasswordForm> m_passwordForm;

    bool m_isScrollingWithAnimationToPageExtent { false };
    std::optional<WebCore::ScrollDirection> m_animatedKeyboardScrollingDirection;
#endif

    Vector<WebCore::ElementIdentifier> m_scrollSnapIdentifiers;
    std::optional<PDFDocumentLayout::PageIndex> m_currentlySnappedPage;

    Vector<WebCore::FloatRect> m_findMatchRectsInDocumentCoordinates;

    RefPtr<AsyncPDFRenderer> m_asyncRenderer;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    std::unique_ptr<PDFDataDetectorOverlayController> m_dataDetectorOverlayController;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::UnifiedPDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isUnifiedPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(UNIFIED_PDF)
