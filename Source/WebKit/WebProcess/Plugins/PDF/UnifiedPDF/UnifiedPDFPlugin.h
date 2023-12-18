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

namespace WebKit {

struct PDFContextMenu;
class WebFrame;
class WebMouseEvent;

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

private:
    explicit UnifiedPDFPlugin(WebCore::HTMLPlugInElement&);
    bool isUnifiedPDFPlugin() const override { return true; }

    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::GraphicsLayer; }
    WebCore::GraphicsLayer* graphicsLayer() const override;

    void teardown() override;

    void installPDFDocument() override;

    CGFloat scaleFactor() const override;

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

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const override;
    WebCore::FloatSize pdfDocumentSizeForPrinting() const override;

    void scheduleRenderingUpdate();

    void updateLayout();
    void geometryDidChange(const WebCore::IntSize&, const WebCore::AffineTransform&) override;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const override;

    bool wantsWheelEvents() const override { return false; }
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override { return false; }
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    bool handleEditingCommand(StringView commandName) override;
    bool isEditingCommandEnabled(StringView commandName) override;

    enum class ContextMenuItemTag : uint8_t {
        OpenWithPreview,
        SinglePage,
        SinglePageContinuous,
        TwoPages,
        TwoPagesContinuous
    };

#if PLATFORM(MAC)
    PDFContextMenu createContextMenu(const WebCore::IntPoint& contextMenuPoint) const;
    void performContextMenuAction(ContextMenuItemTag);

    ContextMenuItemTag contextMenuItemTagFromDisplyMode(const PDFDocumentLayout::DisplayMode& displayMode) const
    {
        switch (displayMode) {
        case PDFDocumentLayout::DisplayMode::SinglePage: return ContextMenuItemTag::SinglePage;
        case PDFDocumentLayout::DisplayMode::Continuous: return ContextMenuItemTag::SinglePageContinuous;
        case PDFDocumentLayout::DisplayMode::TwoUp: return ContextMenuItemTag::TwoPages;
        case PDFDocumentLayout::DisplayMode::TwoUpContinuous: return ContextMenuItemTag::TwoPagesContinuous;
        }
    }
    PDFDocumentLayout::DisplayMode displayModeFromContextMenuItemTag(const ContextMenuItemTag& tag)
    {
        ASSERT(tag == ContextMenuItemTag::SinglePage || tag == ContextMenuItemTag::SinglePageContinuous || tag == ContextMenuItemTag::TwoPages || tag == ContextMenuItemTag::TwoPagesContinuous);
        switch (tag) {
        case ContextMenuItemTag::SinglePage: return PDFDocumentLayout::DisplayMode::SinglePage;
        case ContextMenuItemTag::SinglePageContinuous: return PDFDocumentLayout::DisplayMode::Continuous;
        case ContextMenuItemTag::TwoPages: return PDFDocumentLayout::DisplayMode::TwoUp;
        case ContextMenuItemTag::TwoPagesContinuous: return PDFDocumentLayout::DisplayMode::TwoUpContinuous;
        default:
            ASSERT_NOT_REACHED();
            return PDFDocumentLayout::DisplayMode::Continuous;
        }
    }
    static constexpr int invalidContextMenuItemTag { -1 };
#endif

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
    float pageScaleFactor() const override;

    void updateLayerHierarchy();

    void didChangeScrollOffset() override;
    void didChangeIsInWindow();

    void didChangeSettings() override;

    bool usesAsyncScrolling() const final { return true; }
    WebCore::ScrollingNodeID scrollingNodeID() const final { return m_scrollingNodeID; }

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;
    void updateScrollingExtents();
    WebCore::ScrollingCoordinator* scrollingCoordinator();

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
    void save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&) final;
    void openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&) final;
#endif

    RefPtr<WebCore::GraphicsLayer> createGraphicsLayer(const String& name, WebCore::GraphicsLayer::Type);

    WebCore::IntPoint convertFromPluginToDocument(const WebCore::IntPoint&) const;
    std::optional<PDFDocumentLayout::PageIndex> nearestPageIndexForDocumentPoint(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromDocumentToPage(const WebCore::IntPoint&, PDFDocumentLayout::PageIndex) const;
    PDFElementTypes pdfElementTypesForPluginPoint(const WebCore::IntPoint&) const;

    bool isTaggedPDF() const;

    PDFDocumentLayout m_documentLayout;
    RefPtr<WebCore::GraphicsLayer> m_rootLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrollContainerLayer;
    RefPtr<WebCore::GraphicsLayer> m_scrolledContentsLayer;
    RefPtr<WebCore::GraphicsLayer> m_contentsLayer;

    WebCore::ScrollingNodeID m_scrollingNodeID { 0 };

    float m_scaleFactor { 1 };
    bool m_inMagnificationGesture { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::UnifiedPDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isUnifiedPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(UNIFIED_PDF)
