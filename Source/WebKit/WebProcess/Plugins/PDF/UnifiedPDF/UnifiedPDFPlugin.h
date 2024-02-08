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
#include <WebCore/GraphicsLayer.h>
#include <wtf/OptionSet.h>

OBJC_CLASS WKPDFFormMutationObserver;

namespace WebCore {
enum class DelegatedScrollingMode : uint8_t;
}

namespace WebKit {

struct PDFContextMenu;
class WebFrame;
class WebMouseEvent;
enum class WebEventType : uint8_t;
enum class WebMouseEventButton : int8_t;

class AnnotationTrackingState {
public:
    void startAnnotationTracking(RetainPtr<PDFAnnotation>&&, const WebEventType&, const WebMouseEventButton&);
    void finishAnnotationTracking(const WebEventType&, const WebMouseEventButton&);
    const PDFAnnotation *trackedAnnotation() const { return m_trackedAnnotation.get(); }
    bool isBeingHovered() const;
private:
    void handleMouseDraggedOffTrackedAnnotation();
    void resetAnnotationTrackingState();
    RetainPtr<PDFAnnotation> m_trackedAnnotation;
    bool m_isBeingHovered { false };
};

enum class WebEventModifier : uint8_t;

class UnifiedPDFPlugin final : public PDFPluginBase, public WebCore::GraphicsLayerClient {
public:
    static Ref<UnifiedPDFPlugin> create(WebCore::HTMLPlugInElement&);

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

    CGRect pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>&) const final;
    void setActiveAnnotation(RetainPtr<PDFAnnotation>&&) final;
    void focusNextAnnotation() final;
    void focusPreviousAnnotation() final;

    void attemptToUnlockPDF(const String& password) final;
    void windowActivityDidChange() final;

    float documentFittingScale() const { return m_documentLayout.scale(); }

private:
    explicit UnifiedPDFPlugin(WebCore::HTMLPlugInElement&);
    bool isUnifiedPDFPlugin() const override { return true; }

    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::GraphicsLayer; }
    WebCore::GraphicsLayer* graphicsLayer() const override;

    void teardown() override;

    void installPDFDocument() override;

    float scaleForActualSize() const;

    CGFloat scaleFactor() const override;
    CGSize contentSizeRespectingZoom() const final;

    void didBeginMagnificationGesture() override;
    void didEndMagnificationGesture() override;
    void setPageScaleFactor(double scale, std::optional<WebCore::IntPoint> origin) final;

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

    WebCore::IntSize documentSize() const;
    WebCore::IntSize contentsSize() const override;
    unsigned firstPageHeight() const override;

    void scheduleRenderingUpdate();

    void updateLayout();

    WebCore::IntRect availableContentsRect() const;

    WebCore::DelegatedScrollingMode scrollingMode() const;

    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;
    void updateScrollbars() override;
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
    bool forwardEditingCommandToEditor(const String& commandName, const String& argument) const;
    void selectAll();

    enum class ContextMenuItemTag : uint8_t {
        OpenWithPreview,
        SinglePage,
        SinglePageContinuous,
        TwoPages,
        TwoPagesContinuous,
        ZoomIn,
        ZoomOut,
        ActualSize,
    };

#if PLATFORM(MAC)
    PDFContextMenu createContextMenu(const WebCore::IntPoint& contextMenuPoint) const;
    void performContextMenuAction(ContextMenuItemTag);

    ContextMenuItemTag contextMenuItemTagFromDisplayMode(const PDFDocumentLayout::DisplayMode&) const;
    PDFDocumentLayout::DisplayMode displayModeFromContextMenuItemTag(const ContextMenuItemTag&) const;
    static constexpr int invalidContextMenuItemTag { -1 };
#endif

    // Selections
    enum class SelectionGranularity : uint8_t {
        Character,
        Word,
        Line,
    };
    SelectionGranularity selectionGranularityForMouseEvent(const WebMouseEvent&) const;
    void beginTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::IntPoint& pagePoint, SelectionGranularity, OptionSet<WebEventModifier>);
    void extendCurrentSelectionIfNeeded();
    void continueTrackingSelection(PDFDocumentLayout::PageIndex, const WebCore::IntPoint& pagePoint);
    void setCurrentSelection(RetainPtr<PDFSelection>&&);

    String getSelectionString() const override;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const override;
    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const override;

    id accessibilityHitTest(const WebCore::IntPoint&) const override;
    id accessibilityObject() const override;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const override;

    void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) override;

    // GraphicsLayerClient
    void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::FloatRect&, OptionSet<WebCore::GraphicsLayerPaintBehavior>) override;
    float deviceScaleFactor() const override;
    float pageScaleFactor() const override { return scaleFactor(); }
    bool layerNeedsPlatformContext(const WebCore::GraphicsLayer*) const override { return true; }

    void paintPDFContent(WebCore::GraphicsContext&, const WebCore::FloatRect& clipRect);
    void paintPDFOverlays(WebCore::GraphicsContext&);
    void ensureLayers();
    void updateLayerHierarchy();

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

    // ScrollableArea
    bool requestScrollToPosition(const WebCore::ScrollPosition&, const WebCore::ScrollPositionChangeOptions& = WebCore::ScrollPositionChangeOptions::createProgrammatic()) override;
    bool requestStartKeyboardScrollAnimation(const WebCore::KeyboardScroll& scrollData) override;
    bool requestStopKeyboardScrollAnimation(bool immediate) override;

    float sidePaddingWidth() const;

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
#endif

    void didClickLinkAnnotation(const PDFAnnotation *);

    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String& name, WebCore::GraphicsLayer::Type);

    WebCore::IntPoint convertFromPluginToDocument(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromDocumentToPlugin(const WebCore::IntPoint&) const;
    std::optional<PDFDocumentLayout::PageIndex> pageIndexForDocumentPoint(const WebCore::IntPoint&) const;
    RetainPtr<PDFAnnotation> annotationForRootViewPoint(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromDocumentToPage(const WebCore::IntPoint&, PDFDocumentLayout::PageIndex) const;
    WebCore::IntPoint convertFromPageToDocument(const WebCore::IntPoint&, PDFDocumentLayout::PageIndex) const;
    PDFElementTypes pdfElementTypesForPluginPoint(const WebCore::IntPoint&) const;

    bool isTaggedPDF() const;

    PDFDocumentLayout m_documentLayout;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrollContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrolledContentsLayer;
    RefPtr<WebCore::GraphicsLayer> m_contentsLayer;

    RefPtr<WebCore::GraphicsLayer> m_overflowControlsContainer;
    RefPtr<WebCore::GraphicsLayer> m_layerForHorizontalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForVerticalScrollbar;
    RefPtr<WebCore::GraphicsLayer> m_layerForScrollCorner;

    WebCore::ScrollingNodeID m_scrollingNodeID { 0 };

    float m_scaleFactor { 1 };
    bool m_inMagnificationGesture { false };

    AnnotationTrackingState m_annotationTrackingState;

    struct SelectionTrackingData {
        bool shouldExtendCurrentSelection { false };
        bool shouldMakeMarqueeSelection { false };
        SelectionGranularity granularity { SelectionGranularity::Character };
        PDFDocumentLayout::PageIndex startPageIndex;
        WebCore::IntPoint startPagePoint;
        RetainPtr<PDFSelection> selectionToExtendWith;
        WebCore::IntRect marqueeSelectionRect;
    };
    SelectionTrackingData m_selectionTrackingData;
    RetainPtr<PDFSelection> m_currentSelection;

    RetainPtr<WKPDFFormMutationObserver> m_pdfMutationObserver;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::UnifiedPDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isUnifiedPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(UNIFIED_PDF)
