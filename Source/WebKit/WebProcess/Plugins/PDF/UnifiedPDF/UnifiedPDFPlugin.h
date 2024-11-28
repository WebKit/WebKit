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
#include "PDFPageCoverage.h"
#include "PDFPluginBase.h"
#include <WebCore/ElementIdentifier.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Page.h>
#include <WebCore/Timer.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS PDFAction;
OBJC_CLASS PDFDestination;
OBJC_CLASS PDFPage;
OBJC_CLASS WKPDFFormMutationObserver;

namespace WTF {
class TextStream;
}

namespace WebCore {
class FrameView;
class PageOverlay;
class PlatformWheelEvent;
class ShadowRoot;

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
class PDFPresentationController;
class WebFrame;
class WebMouseEvent;
struct PDFContextMenu;
struct PDFContextMenuItem;

enum class WebEventType : uint8_t;
enum class WebMouseEventButton : int8_t;
enum class WebEventModifier : uint8_t;

enum class RepaintRequirement : uint8_t {
    PDFContent      = 1 << 0,
    Selection       = 1 << 1,
    HoverOverlay    = 1 << 2
};
using RepaintRequirements = OptionSet<RepaintRequirement>;

class AnnotationTrackingState {
public:
    RepaintRequirements startAnnotationTracking(RetainPtr<PDFAnnotation>&&, WebEventType, WebMouseEventButton);
    RepaintRequirements finishAnnotationTracking(PDFAnnotation* annotationUnderMouse, WebEventType, WebMouseEventButton);

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
    WTF_MAKE_TZONE_ALLOCATED(UnifiedPDFPlugin);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(UnifiedPDFPlugin);

    friend class AsyncPDFRenderer;
    friend class PDFPresentationController;
    friend class PDFScrollingPresentationController;
    friend class PDFDiscretePresentationController;
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
    void setActiveAnnotation(SetActiveAnnotationParams&&) final;
    void focusNextAnnotation() final;
    void focusPreviousAnnotation() final;
#if PLATFORM(MAC)
    RetainPtr<PDFAnnotation> nextTextAnnotation(AnnotationSearchDirection) const;
    enum class ShouldPerformGoToAction : bool { No, Yes };
    void handlePDFActionForAnnotation(PDFAnnotation *, PDFDocumentLayout::PageIndex currentPageIndex, ShouldPerformGoToAction);
#endif
    enum class IsAnnotationCommit : bool { No, Yes };
    static RepaintRequirements repaintRequirementsForAnnotation(PDFAnnotation *, IsAnnotationCommit = IsAnnotationCommit::No);
    void repaintAnnotationsForFormField(NSString *fieldName);

    Vector<WebCore::FloatRect> annotationRectsForTesting() const final;
    void setTextAnnotationValueForTesting(unsigned pageIndex, unsigned annotationIndex, const String& value) final;

    void attemptToUnlockPDF(const String& password) final;
    void windowActivityDidChange() final;

    void didSameDocumentNavigationForFrame(WebFrame&) final;

    float documentFittingScale() const { return m_documentLayout.scale(); }

    bool shouldCachePagePreviews() const;

#if PLATFORM(MAC)
    WebCore::FloatRect convertFromPDFPageToScreenForAccessibility(const WebCore::FloatRect&, PDFDocumentLayout::PageIndex) const;
    void accessibilityScrollToPage(PDFDocumentLayout::PageIndex);
#endif

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    void installDataDetectorOverlay(WebCore::PageOverlay&);
    void uninstallDataDetectorOverlay(WebCore::PageOverlay&);

    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&);

    bool canShowDataDetectorHighlightOverlays() const;
#endif

    void scheduleRenderingUpdate(OptionSet<WebCore::RenderingUpdateStep> = WebCore::RenderingUpdateStep::LayerFlush);
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayerClient&); // Why public?
    float deviceScaleFactor() const override;

    WebCore::FloatRect rectForSelectionInMainFrameContentsSpace(PDFSelection *) const;

    /*
        Unified PDF Plugin coordinate spaces, in depth order:

        - "root view": same as the rest of WebKit.

        - "plugin": the space of the plugin element (origin at the top left,
            ignoring all internal transforms).

        - "contents": the space of the contents layer, with scrolling subtracted
            out and page scale multiplied in. In scrolling mode, the same as the
            painting space. In discrete mode, with variable sized pages, use
            convertFromContentsToPainting/convertFromPaintingToContents to go
            to and from painting space.

        - "document": the space that the PDF pages are laid down in, with
            PDFDocumentLayout's width-fitting scale divided out; includes margins.

        - "page": the space of each actual PDFPage, as used by PDFKit; origin at
            the bottom left of the crop box; page rotation multiplied in.
    */

    enum class CoordinateSpace : uint8_t {
        PDFPage,
        PDFDocumentLayout,
        Contents,
        ScrolledContents,
        Plugin
    };

    RetainPtr<PDFPage> pageAtIndex(PDFDocumentLayout::PageIndex) const;

    void setPDFDisplayModeForTesting(const String&) final;

    double minScaleFactor() const final;
    double maxScaleFactor() const final;

private:
    explicit UnifiedPDFPlugin(WebCore::HTMLPlugInElement&);
    bool isUnifiedPDFPlugin() const override { return true; }

    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::GraphicsLayer; }
    WebCore::GraphicsLayer* graphicsLayer() const override;

    void teardown() override;

    void incrementalLoadingDidProgress() override;
    void incrementalLoadingDidCancel() override;
    void incrementalLoadingDidFinish() override;

    void installPDFDocument() override;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    void enableDataDetection();
    void didInvalidateDataDetectorHighlightOverlayRects();

    PDFDataDetectorOverlayController& dataDetectorOverlayController() { return *m_dataDetectorOverlayController; }
#endif

    const PDFDocumentLayout& documentLayout() const { return m_documentLayout; }

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
    double nonNormalizedScaleFactor() const { return m_scaleFactor; }

    // Scale normalization is used to map the internal "scale factor" to the exposed scaleFactor()/setPageScaleFactor()
    // so that scale factor 1 shows at "Actual Size".
    void computeNormalizationFactor();
    double fromNormalizedScaleFactor(double) const;
    double toNormalizedScaleFactor(double) const;

    void didBeginMagnificationGesture() override;
    void didEndMagnificationGesture() override;
    void setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin) final;
    void setScaleFactor(double scale, std::optional<WebCore::IntPoint> origin = std::nullopt);

    enum class CheckForMagnificationGesture : bool { No, Yes };
    void deviceOrPageScaleFactorChanged(CheckForMagnificationGesture = CheckForMagnificationGesture::No);

    WebCore::IntSize documentSize() const;
    WebCore::IntSize contentsSize() const override;
    bool visibleOrDocumentSizeIsEmpty() const { return m_size.isEmpty() || documentSize().isEmpty(); }
    unsigned firstPageHeight() const override;
    unsigned heightForPageAtIndex(PDFDocumentLayout::PageIndex) const;
    WebCore::FloatRect layoutBoundsForPageAtIndex(PDFDocumentLayout::PageIndex) const;

    enum class AdjustScaleAfterLayout : bool {
        No,
        Yes
    };
    void updateLayout(AdjustScaleAfterLayout = AdjustScaleAfterLayout::No, std::optional<ShouldUpdateAutoSizeScale> shouldUpdateAutoSizeScaleOverride = { });

    WebCore::IntRect availableContentsRect() const;

    WebCore::DelegatedScrollingMode scrollingMode() const;
    bool isFullMainFramePlugin() const;

    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;
    void updateScrollbars() override;
    void willAttachScrollingNode() final;
    void didAttachScrollingNode() final;

    bool geometryDidChange(const WebCore::IntSize&, const WebCore::AffineTransform&) override;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const override;

    NSData *liveData() const override;

    void releaseMemory() override;

    bool wantsWheelEvents() const override;
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override;
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

    void setDisplayMode(PDFDocumentLayout::DisplayMode);
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
    enum class IsMarqueeSelection : bool { No, Yes };

    SelectionGranularity selectionGranularityForMouseEvent(const WebMouseEvent&) const;
    void beginTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::FloatPoint& pagePoint, const WebMouseEvent&);
    void extendCurrentSelectionIfNeeded();
    void updateCurrentSelectionForContextMenuEventIfNeeded();
    void continueTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::FloatPoint& pagePoint, IsDraggingSelection);
    void freezeCursorDuringSelectionDragIfNeeded(IsDraggingSelection, IsMarqueeSelection);
    void unfreezeCursorAfterSelectionDragIfNeeded();
    void stopTrackingSelection();
    void setCurrentSelection(RetainPtr<PDFSelection>&&);
    RetainPtr<PDFSelection> protectedCurrentSelection() const;
    void repaintOnSelectionChange(ActiveStateChangeReason, PDFSelection *previousSelection = nil);
    void showOrHideSelectionLayerAsNecessary();

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

    Vector<WebFoundTextRange::PDFData> findTextMatches(const String&, WebCore::FindOptions) final;
    Vector<WebCore::FloatRect> rectsForTextMatch(const WebFoundTextRange::PDFData&) final;
    RefPtr<WebCore::TextIndicator> textIndicatorForTextMatch(const WebFoundTextRange::PDFData&, WebCore::TextIndicatorPresentationTransition) final;
    void scrollToRevealTextMatch(const WebFoundTextRange::PDFData&) final;

    Vector<WebCore::FloatRect> visibleRectsForFindMatchRects(PDFPageCoverage) const;
    PDFSelection *selectionFromWebFoundTextRangePDFData(const WebFoundTextRange::PDFData&) const;

    RefPtr<WebCore::TextIndicator> textIndicatorForCurrentSelection(OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition) final;
    RefPtr<WebCore::TextIndicator> textIndicatorForSelection(PDFSelection *, OptionSet<WebCore::TextIndicatorOption>, WebCore::TextIndicatorPresentationTransition);

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;

    enum class FirstPageOnly : bool { No, Yes };
    PDFPageCoverage pageCoverageForSelection(PDFSelection *, FirstPageOnly = FirstPageOnly::No) const;

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

    enum class PaintingBehavior : bool { All, PageContentsOnly };
    void paintPDFContent(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, const std::optional<PDFLayoutRow>& = { }, PaintingBehavior = PaintingBehavior::All, AsyncPDFRenderer* = nullptr);
#if ENABLE(UNIFIED_PDF_SELECTION_LAYER)
    void paintPDFSelection(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect, std::optional<PDFLayoutRow> = { });
#endif
    bool canPaintSelectionIntoOwnedLayer() const;

    void willChangeVisibleRow();
    void didChangeVisibleRow();

    void ensureLayers();
    void updateLayerHierarchy();

    void incrementalLoadingRepaintTimerFired();
    void repaintForIncrementalLoad();

    void didChangeScrollOffset() override;
    void didChangeIsInWindow() override;
    bool isInWindow() const;

    void didChangeSettings() override;

    void createScrollbarsController() override;

    bool usesAsyncScrolling() const final { return true; }
    std::optional<WebCore::ScrollingNodeID> scrollingNodeID() const final { return m_scrollingNodeID; }

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;

    WebCore::GraphicsLayer* layerForHorizontalScrollbar() const override;
    WebCore::GraphicsLayer* layerForVerticalScrollbar() const override;
    WebCore::GraphicsLayer* layerForScrollCorner() const override;

    void updateScrollingExtents();

    bool updateOverflowControlsLayers(bool needsHorizontalScrollbarLayer, bool needsVerticalScrollbarLayer, bool needsScrollCornerLayer);
    void positionOverflowControlsLayers();

    void createScrollingNodeIfNecessary();

    void revealPDFDestination(PDFDestination *);
    void revealPointInPage(WebCore::FloatPoint pointInPDFPageSpace, PDFDocumentLayout::PageIndex);
    void revealRectInPage(const WebCore::FloatRect& rectInPDFPageSpace, PDFDocumentLayout::PageIndex);
    void revealPage(PDFDocumentLayout::PageIndex);
    void revealFragmentIfNeeded();

    // Only use this if some other function has ensured that the correct page is visible.
    void scrollToPointInContentsSpace(WebCore::FloatPoint);

    OptionSet<WebCore::TiledBackingScrollability> computeScrollability() const;

    // ScrollableArea
    bool requestScrollToPosition(const WebCore::ScrollPosition&, const WebCore::ScrollPositionChangeOptions& = WebCore::ScrollPositionChangeOptions::createProgrammatic()) override;
    bool requestStartKeyboardScrollAnimation(const WebCore::KeyboardScroll& scrollData) override;
    bool requestStopKeyboardScrollAnimation(bool immediate) override;

    WebCore::OverscrollBehavior overscrollBehavior() const;
    WebCore::OverscrollBehavior horizontalOverscrollBehavior() const override { return overscrollBehavior(); }
    WebCore::OverscrollBehavior verticalOverscrollBehavior() const override { return overscrollBehavior(); }

    WebCore::FloatSize centeringOffset() const;

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
    void resetZoom();
#endif

    bool supportsPasswordForm() const;
    void installAnnotationContainer();

    std::optional<PDFDocumentLayout::PageIndex> pageIndexForAnnotation(PDFAnnotation *) const;
    std::optional<PDFDocumentLayout::PageIndex> pageIndexWithHoveredAnnotation() const;
    void paintHoveredAnnotationOnPage(PDFDocumentLayout::PageIndex, WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect);

    void followLinkAnnotation(PDFAnnotation *);

    void startTrackingAnnotation(RetainPtr<PDFAnnotation>&&, WebEventType, WebMouseEventButton);
    void updateTrackedAnnotation(PDFAnnotation *annotationUnderMouse);
    void finishTrackingAnnotation(PDFAnnotation *annotationUnderMouse, WebEventType, WebMouseEventButton, RepaintRequirements = { });

    void revealAnnotation(PDFAnnotation *);

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() const;
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(GraphicsLayerClient&, WebCore::GraphicsLayer::Type);
    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String& name, WebCore::GraphicsLayer::Type);

    void setNeedsRepaintForAnnotation(PDFAnnotation *, RepaintRequirements);
    void setNeedsRepaintInDocumentRect(RepaintRequirements, const WebCore::FloatRect&, std::optional<PDFLayoutRow>);

    // "Up" is inside-out.
    template <typename T>
    T convertUp(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex = { }) const;

    // "Down" is outside-in.
    template <typename T>
    T convertDown(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> = { }) const;

    // Painting coordinates are a "branch" off the linear CoordinateSpace list, but only different from "Contents" for variable-page-sized PDFs in discrete mode.
    WebCore::FloatRect convertFromContentsToPainting(const WebCore::FloatRect&, std::optional<PDFDocumentLayout::PageIndex> = { }) const;
    WebCore::FloatRect convertFromPaintingToContents(const WebCore::FloatRect&, std::optional<PDFDocumentLayout::PageIndex> = { }) const;

    PDFDocumentLayout::PageIndex indexForCurrentPageInView() const;

    RetainPtr<PDFAnnotation> annotationForRootViewPoint(const WebCore::IntPoint&) const;

    PDFElementTypes pdfElementTypesForPluginPoint(const WebCore::IntPoint&) const;

    bool isTaggedPDF() const;

    bool shouldShowDebugIndicators() const;

    float scaleForPagePreviews() const;

    void createPasswordEntryForm();
    void teardownPasswordEntryForm() override;

    bool isInDiscreteDisplayMode() const;
    bool isShowingTwoPages() const;

    WebCore::PlatformWheelEvent wheelEventCopyWithVelocity(const WebCore::PlatformWheelEvent&) const;

    void setPresentationController(RefPtr<PDFPresentationController>&&);

    WebCore::FloatRect pageBoundsInContentsSpace(PDFDocumentLayout::PageIndex) const;

    RefPtr<PDFPresentationController> m_presentationController;

    PDFDocumentLayout m_documentLayout;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrollContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrolledContentsLayer;

    RefPtr<WebCore::GraphicsLayer> m_overflowControlsContainer;
    RefPtr<WebCore::GraphicsLayer> m_layerForHorizontalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForVerticalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForScrollCorner;

    Markable<WebCore::ScrollingNodeID> m_scrollingNodeID;

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

    WebCore::Timer m_incrementalLoadingRepaintTimer { *this, &UnifiedPDFPlugin::incrementalLoadingRepaintTimerFired };

    RetainPtr<WKPDFFormMutationObserver> m_pdfMutationObserver;

    RefPtr<PDFPluginPasswordField> m_passwordField;
    RefPtr<PDFPluginPasswordForm> m_passwordForm;

    PDFPageCoverage m_findMatchRects;

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    std::unique_ptr<PDFDataDetectorOverlayController> m_dataDetectorOverlayController;
#endif

    RefPtr<WebCore::ShadowRoot> m_shadowRoot;

    // FIXME: We should rationalize these with the values in ViewGestureController.
    // For now, we'll leave them differing as they do in PDFPlugin.
    static constexpr double minimumZoomScale = 0.2;
    static constexpr double maximumZoomScale = 6.0;
};

WTF::TextStream& operator<<(WTF::TextStream&, RepaintRequirement);

template <typename T>
T UnifiedPDFPlugin::convertDown(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    static_assert(std::is_same<T, WebCore::FloatPoint>::value || std::is_same<T, WebCore::FloatRect>::value, "Coordinate conversion should use float types");
    auto mappedValue = sourceValue;

    switch (sourceSpace) {
    case CoordinateSpace::Plugin:
        if (destinationSpace == CoordinateSpace::Plugin)
            return mappedValue;

        mappedValue.moveBy(WebCore::FloatPoint { m_scrollOffset });
        FALLTHROUGH;

    case CoordinateSpace::ScrolledContents:
        if (destinationSpace == CoordinateSpace::ScrolledContents)
            return mappedValue;

        mappedValue.scale(1 / m_scaleFactor);
        mappedValue.move(-centeringOffset());
        FALLTHROUGH;

    case CoordinateSpace::Contents:
        if (destinationSpace == CoordinateSpace::Contents)
            return mappedValue;

        mappedValue.scale(1 / m_documentLayout.scale());
        FALLTHROUGH;

    case CoordinateSpace::PDFDocumentLayout:
        if (destinationSpace == CoordinateSpace::PDFDocumentLayout)
            return mappedValue;

        ASSERT(pageIndex);
        ASSERT(*pageIndex < m_documentLayout.pageCount());
        mappedValue = m_documentLayout.documentToPDFPage(mappedValue, *pageIndex);
        FALLTHROUGH;

    case CoordinateSpace::PDFPage:
        if (destinationSpace == CoordinateSpace::PDFPage)
            return mappedValue;
    }

    ASSERT_NOT_REACHED();
    return mappedValue;
}

template <typename T>
T UnifiedPDFPlugin::convertUp(CoordinateSpace sourceSpace, CoordinateSpace destinationSpace, T sourceValue, std::optional<PDFDocumentLayout::PageIndex> pageIndex) const
{
    static_assert(std::is_same<T, WebCore::FloatPoint>::value || std::is_same<T, WebCore::FloatRect>::value, "Coordinate conversion should use float types");
    auto mappedValue = sourceValue;

    switch (sourceSpace) {
    case CoordinateSpace::PDFPage:
        if (destinationSpace == CoordinateSpace::PDFPage)
            return mappedValue;

        ASSERT(pageIndex);
        ASSERT(*pageIndex < m_documentLayout.pageCount());
        mappedValue = m_documentLayout.pdfPageToDocument(mappedValue, *pageIndex);
        FALLTHROUGH;

    case CoordinateSpace::PDFDocumentLayout:
        if (destinationSpace == CoordinateSpace::PDFDocumentLayout)
            return mappedValue;

        mappedValue.scale(m_documentLayout.scale());
        FALLTHROUGH;

    case CoordinateSpace::Contents:
        if (destinationSpace == CoordinateSpace::Contents)
            return mappedValue;

        mappedValue.move(centeringOffset());
        mappedValue.scale(m_scaleFactor);
        FALLTHROUGH;

    case CoordinateSpace::ScrolledContents:
        if (destinationSpace == CoordinateSpace::ScrolledContents)
            return mappedValue;

        mappedValue.moveBy(-WebCore::FloatPoint { m_scrollOffset });
        FALLTHROUGH;

    case CoordinateSpace::Plugin:
        if (destinationSpace == CoordinateSpace::Plugin)
            return mappedValue;
    }

    ASSERT_NOT_REACHED();
    return mappedValue;
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::UnifiedPDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isUnifiedPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(UNIFIED_PDF)
