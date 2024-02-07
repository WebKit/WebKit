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

#include "DataReference.h"
#include "FrameInfoData.h"
#include "PDFPluginIdentifier.h"
#include "PDFScriptEvaluator.h"
#include "WebMouseEvent.h"
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

OBJC_CLASS NSDictionary;
OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;

namespace WebCore {
class FragmentedSharedBuffer;
class GraphicsContext;
class Element;
class HTMLPlugInElement;
class NetscapePlugInStreamLoaderClient;
class ResourceResponse;
class Scrollbar;
class ShareableBitmap;
class SharedBuffer;
enum class PlatformCursorType : uint8_t;
}

namespace WebKit {

class PDFIncrementalLoader;
class PDFPluginAnnotation;
class PluginView;
class WebFrame;
class WebKeyboardEvent;
class WebMouseEvent;
class WebWheelEvent;
struct WebHitTestResultData;

class PDFPluginBase : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<PDFPluginBase>, public WebCore::ScrollableArea, public PDFScriptEvaluator::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PDFPluginBase);
    friend class PDFIncrementalLoader;
public:
    static WebCore::PluginInfo pluginInfo();

    virtual ~PDFPluginBase();

    using WebKit::PDFScriptEvaluator::Client::weakPtrFactory;
    using WebKit::PDFScriptEvaluator::Client::WeakValueType;
    using WebKit::PDFScriptEvaluator::Client::WeakPtrImplType;

    void startLoading();
    void destroy();

    virtual bool isUnifiedPDFPlugin() const { return false; }
    virtual bool isLegacyPDFPlugin() const { return false; }

    PDFPluginIdentifier identifier() const { return m_identifier; }

    virtual WebCore::PluginLayerHostingStrategy layerHostingStrategy() const = 0;
    virtual PlatformLayer* platformLayer() const { return nullptr; }
    virtual WebCore::GraphicsLayer* graphicsLayer() const { return nullptr; }

    virtual void setView(PluginView&);

    virtual void willDetachRenderer();

    virtual bool isComposited() const { return false; }

    virtual bool shouldCreateTransientPaintingSnapshot() const { return false; }
    virtual RefPtr<WebCore::ShareableBitmap> snapshot() { return nullptr; }
    virtual void paint(WebCore::GraphicsContext&, const WebCore::IntRect&) { }

    virtual CGFloat scaleFactor() const = 0;
    virtual CGSize contentSizeRespectingZoom() const = 0;

    virtual CGFloat minScaleFactor() const { return 0.25; }
    virtual CGFloat maxScaleFactor() const { return 5; }

    bool isLocked() const;

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const { return m_pdfDocument; }
    WebCore::FloatSize pdfDocumentSizeForPrinting() const;

    virtual bool geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform);
    virtual void visibilityDidChange(bool);
    virtual void deviceScaleFactorChanged(float) { }

    bool handlesPageScaleFactor() const;
    virtual void didBeginMagnificationGesture() { }
    virtual void didEndMagnificationGesture() { }
    virtual void setPageScaleFactor(double, std::optional<WebCore::IntPoint> origin) = 0;

    void updateControlTints(WebCore::GraphicsContext&);

    virtual RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const = 0;

    virtual bool wantsWheelEvents() const = 0;
    virtual bool handleMouseEvent(const WebMouseEvent&) = 0;
    virtual bool handleWheelEvent(const WebWheelEvent&) = 0;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) = 0;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) = 0;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) = 0;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) = 0;
    virtual bool handleEditingCommand(const String& commandName, const String& argument) = 0;
    virtual bool isEditingCommandEnabled(const String& commandName) = 0;

    virtual String getSelectionString() const = 0;
    virtual bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const = 0;
    virtual WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const = 0;

    virtual unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;
    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) = 0;

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) = 0;
    virtual std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const = 0;

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
    WebCore::IntRect convertFromRootViewToPlugin(const WebCore::IntRect&) const;
    WebCore::IntPoint convertFromPluginToRootView(const WebCore::IntPoint&) const;
    WebCore::IntRect convertFromPluginToRootView(const WebCore::IntRect&) const;
    WebCore::IntRect boundsOnScreen() const;

    WebCore::ScrollPosition scrollPositionForTesting() const { return scrollPosition(); }
    WebCore::Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }

    virtual void didChangeSettings() { }

    // HUD Actions.
#if ENABLE(PDF_HUD)
    virtual void zoomIn() = 0;
    virtual void zoomOut() = 0;
    void save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&);
    void openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&);
#endif

    void notifyCursorChanged(WebCore::PlatformCursorType);

    WebCore::ScrollPosition scrollPosition() const final;

    virtual void setActiveAnnotation(RetainPtr<PDFAnnotation>&&) = 0;
    void didMutatePDFDocument() { m_pdfDocumentWasMutated = true; }

    virtual CGRect pluginBoundsForAnnotation(RetainPtr<PDFAnnotation>&) const = 0;
    virtual void focusNextAnnotation() = 0;
    virtual void focusPreviousAnnotation() = 0;

    void navigateToURL(const URL&);

    virtual void attemptToUnlockPDF(const String& password) = 0;

#if HAVE(INCREMENTAL_PDF_APIS)
    bool incrementalPDFLoadingEnabled() const { return m_incrementalPDFLoadingEnabled; }
    void receivedNonLinearizedPDFSentinel();
    void startByteRangeRequest(WebCore::NetscapePlugInStreamLoaderClient&, uint64_t requestIdentifier, uint64_t position, size_t count);
    void adoptBackgroundThreadDocument(RetainPtr<PDFDocument>&&);
    void maybeClearHighLatencyDataProviderFlag();
#endif

    void notifySelectionChanged();

    virtual void windowActivityDidChange() { }

private:
    bool documentFinishedLoading() const { return m_documentFinishedLoading; }
    uint64_t streamedBytes() const { return m_streamedBytes; }
    void ensureDataBufferLength(uint64_t);

    bool haveStreamedDataForRange(uint64_t offset, size_t count) const;
    // This just checks whether the CFData is large enough; it doesn't know if we filled this range with data.
    bool haveDataForRange(uint64_t offset, size_t count) const;

    void insertRangeRequestData(uint64_t offset, const Vector<uint8_t>&);

    // Returns the number of bytes copied.
    size_t copyDataAtPosition(void* buffer, uint64_t sourcePosition, size_t count) const;
    // FIXME: It would be nice to avoid having both the "copy into a buffer" and "return a pointer" ways of getting data.
    const uint8_t* dataPtrForRange(uint64_t sourcePosition, size_t count) const;

protected:
    explicit PDFPluginBase(WebCore::HTMLPlugInElement&);

    WebCore::Page* page() const;

    virtual void teardown();

    bool supportsForms();

    void createPDFDocument();
    virtual void installPDFDocument() = 0;
    void tryRunScriptsInPDFDocument();

    virtual unsigned firstPageHeight() const = 0;

    NSData *originalData() const;
    virtual NSData *liveData() const = 0;

    void addArchiveResource();

    void invalidateRect(const WebCore::IntRect&);

    void print() override;

    // ScrollableArea functions.
    WebCore::IntRect scrollCornerRect() const final;
    WebCore::ScrollableArea* enclosingScrollableArea() const final;
    bool isScrollableOrRubberbandable() final { return true; }
    bool hasScrollableOrRubberbandableAncestor() final { return true; }
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    void setScrollOffset(const WebCore::ScrollOffset&) final;
    bool isActive() const final;
    bool isScrollCornerVisible() const final { return false; }
    WebCore::ScrollPosition minimumScrollPosition() const final;
    WebCore::ScrollPosition maximumScrollPosition() const final;
    WebCore::IntSize visibleSize() const final { return m_size; }
    WebCore::IntPoint lastKnownMousePositionInView() const override;

    float deviceScaleFactor() const override;
    bool shouldSuspendScrollAnimations() const final { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;

    WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntRect& scrollbarRect) const final;
    WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntRect& parentRect) const final;
    WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntPoint& scrollbarPoint) const final;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntPoint& parentPoint) const final;

    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;
    bool shouldPlaceVerticalScrollbarOnLeft() const final { return false; }

    WebCore::IntRect viewRelativeVerticalScrollbarRect() const;
    WebCore::IntRect viewRelativeHorizontalScrollbarRect() const;
    WebCore::IntRect viewRelativeScrollCornerRect() const;

    String debugDescription() const final;

    // Scrolling, but not ScrollableArea:
    virtual void didChangeScrollOffset() = 0;
    virtual void updateScrollbars();
    virtual Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    virtual void destroyScrollbar(WebCore::ScrollbarOrientation);

#if ENABLE(PDF_HUD)
    void updatePDFHUDLocation();
    WebCore::IntRect frameForHUDInRootViewCoordinates() const;
    bool hudEnabled() const;
#endif

#if !LOG_DISABLED
    void incrementalLoaderLog(const String&);
#endif

    SingleThreadWeakPtr<PluginView> m_view;
    WeakPtr<WebFrame> m_frame;
    WeakPtr<WebCore::HTMLPlugInElement, WebCore::WeakPtrImplWithEventTargetData> m_element;

    PDFPluginIdentifier m_identifier;

    // m_data grows as we receive data in the primary request (PDFPluginBase::streamDidReceiveData())
    // but also as byte range requests are received via m_incrementalLoader, so it may have "holes"
    // before the main resource is fully loaded.
    RetainPtr<CFMutableDataRef> m_data;
    uint64_t m_streamedBytes { 0 };

    RetainPtr<PDFDocument> m_pdfDocument;

    String m_suggestedFilename;

    WebCore::IntSize m_size;
    WebCore::AffineTransform m_rootViewToPluginTransform;

    WebCore::IntSize m_scrollOffset;
    std::optional<WebMouseEvent> m_lastMouseEvent;

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;

    bool m_documentFinishedLoading { false };
    bool m_isBeingDestroyed { false };
    std::atomic<bool> m_hasBeenDestroyed { false };
    bool m_didRunScripts { false };

#if PLATFORM(MAC)
    RefPtr<PDFPluginAnnotation> m_activeAnnotation;
#endif
    RefPtr<WebCore::Element> m_annotationContainer;
    bool m_pdfDocumentWasMutated { false };

#if HAVE(INCREMENTAL_PDF_APIS)
    RefPtr<PDFIncrementalLoader> m_incrementalLoader;
    std::atomic<bool> m_incrementalPDFLoadingEnabled { false };
#endif

    // Set overflow: hidden on the annotation container so <input> elements scrolled out of view don't show
    // scrollbars on the body. We can't add annotations directly to the body, because overflow: hidden on the body
    // will break rubber-banding.
    static constexpr auto annotationStyle =
    "#annotationContainer {"
    "    overflow: hidden; "
    "    position: absolute; "
    "    pointer-events: none; "
    "    top: 0; "
    "    left: 0; "
    "    right: 0; "
    "    bottom: 0; "
    "    display: -webkit-box; "
    "    -webkit-box-align: center; "
    "    -webkit-box-pack: center; "
    "} "
    ".annotation { "
    "    position: absolute; "
    "    pointer-events: auto; "
    "} "
    "textarea.annotation { "
    "    resize: none; "
    "} "
    "input.annotation[type='password'] { "
    "    position: static; "
    "    width: 238px; "
    "    height: 20px; "
    "    margin-top: 110px; "
    "    font-size: 15px; "
    "} "_s;
};

} // namespace WebKit

#endif // ENABLE(PDF_PLUGIN)
