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

#if ENABLE(PDF_PLUGIN)

#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FloatRect.h>
#include <WebCore/PluginData.h>
#include <WebCore/PluginViewBase.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ScrollableArea.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeTraits.h>
#include <wtf/WeakPtr.h>

// FIXME: These Objective-C classes should only appear in PDFPlugin, not here in the base class.
OBJC_CLASS NSDictionary;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;

namespace WebCore {
class FragmentedSharedBuffer;
class GraphicsContext;
class Element;
class HTMLPlugInElement;
class ResourceResponse;
class Scrollbar;
class SharedBuffer;
}

namespace WebKit {

class PluginView;
class ShareableBitmap;
class WebFrame;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
struct WebHitTestResultData;

class PDFPluginBase : public ThreadSafeRefCounted<PDFPluginBase>, public WebCore::ScrollableArea {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFPluginBase);
public:
    static WebCore::PluginInfo pluginInfo();

    virtual ~PDFPluginBase() = default;

    void destroy();

    virtual bool isUnifiedPDFPlugin() const { return false; }
    virtual bool isLegacyPDFPlugin() const { return false; }

    virtual WebCore::PluginLayerHostingStrategy layerHostingStrategy() const = 0;
    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual WebCore::GraphicsLayer* graphicsLayer() const { return nullptr; }

    virtual void setView(PluginView&);

    virtual void willDetachRenderer();

    virtual bool isComposited() const { return false; }

    virtual void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) { }

    virtual CGFloat scaleFactor() const = 0;

    virtual bool isLocked() const = 0;

    // FIXME: Can we use PDFDocument here?
    virtual RetainPtr<PDFDocument> pdfDocumentForPrinting() const = 0;
    virtual WebCore::FloatSize pdfDocumentSizeForPrinting() const = 0;

    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform);
    virtual void visibilityDidChange(bool) { }
    virtual void contentsScaleFactorChanged(float) { }

    void updateControlTints(WebCore::GraphicsContext&);

    virtual RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const = 0;

    virtual bool wantsWheelEvents() const { return true; }
    virtual bool handleMouseEvent(const WebMouseEvent&) = 0;
    virtual bool handleWheelEvent(const WebWheelEvent&) = 0;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) = 0;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) = 0;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) = 0;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    virtual bool handleEditingCommand(StringView commandName) = 0;
    virtual bool isEditingCommandEnabled(StringView commandName) = 0;

    virtual String getSelectionString() const = 0;
    virtual bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const = 0;
    virtual WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const = 0;

    virtual unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;
    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) = 0;
    virtual std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const = 0;

    virtual RefPtr<ShareableBitmap> snapshot() = 0;

    virtual id accessibilityHitTest(const WebCore::IntPoint&) const = 0;
    virtual id accessibilityObject() const = 0;
    virtual id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const = 0;

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    bool isFullFramePlugin() const;
    WebCore::IntSize size() const { return m_size; }

    void streamDidReceiveResponse(const WebCore::ResourceResponse&);
    void streamDidReceiveData(const WebCore::SharedBuffer&);
    void streamDidFinishLoading();
    void streamDidFail();

    WebCore::IntPoint convertFromRootViewToPlugin(const WebCore::IntPoint&) const;

    WebCore::ScrollPosition scrollPositionForTesting() const { return scrollPosition(); }
    WebCore::Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }

protected:
    explicit PDFPluginBase(WebCore::HTMLPlugInElement&);

    WebCore::Page* page() const;

    virtual void teardown() = 0;

    virtual void createPDFDocument() = 0;
    virtual void installPDFDocument() = 0;
    virtual void tryRunScriptsInPDFDocument() { }

    virtual void incrementalPDFStreamDidReceiveData(const WebCore::SharedBuffer&) { }
    virtual bool incrementalPDFStreamDidFinishLoading() { return false; }
    virtual void incrementalPDFStreamDidFail() { }

    virtual unsigned firstPageHeight() const = 0;

    void ensureDataBufferLength(uint64_t);
    void addArchiveResource();

    void invalidateRect(const WebCore::IntRect&);

    // ScrollableArea functions.
    WebCore::IntRect scrollCornerRect() const final;
    WebCore::ScrollableArea* enclosingScrollableArea() const final;
    bool isScrollableOrRubberbandable() final { return true; }
    bool hasScrollableOrRubberbandableAncestor() final { return true; }
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    void setScrollOffset(const WebCore::ScrollOffset&) final;
    bool isActive() const final;
    bool isScrollCornerVisible() const final { return false; }
    WebCore::ScrollPosition scrollPosition() const final;
    WebCore::ScrollPosition minimumScrollPosition() const final;
    WebCore::ScrollPosition maximumScrollPosition() const final;
    WebCore::IntSize visibleSize() const final { return m_size; }
    float deviceScaleFactor() const override;
    bool shouldSuspendScrollAnimations() const final { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) final;
    WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntRect& scrollbarRect) const final;
    WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntRect& parentRect) const final;
    WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntPoint& scrollbarPoint) const final;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntPoint& parentPoint) const final;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;
    bool shouldPlaceVerticalScrollbarOnLeft() const final { return false; }
    String debugDescription() const final;

    // Scrolling, but not ScrollableArea:
    virtual void didChangeScrollOffset() = 0;
    virtual void updateScrollbars();
    virtual Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    virtual void destroyScrollbar(WebCore::ScrollbarOrientation);

    WeakPtr<PluginView> m_view;
    WeakPtr<WebFrame> m_frame;

    RetainPtr<CFMutableDataRef> m_data;

    String m_suggestedFilename;
    uint64_t m_streamedBytes { 0 };

    WebCore::IntSize m_size;
    WebCore::AffineTransform m_rootViewToPluginTransform;

    WebCore::IntSize m_scrollOffset;

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;

    bool m_documentFinishedLoading { false };
    bool m_isBeingDestroyed { false };
    bool m_hasBeenDestroyed { false };
};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
