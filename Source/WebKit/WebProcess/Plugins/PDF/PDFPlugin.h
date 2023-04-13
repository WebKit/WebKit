/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
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

#if ENABLE(PDFKIT_PLUGIN)

#include "DataReference.h"
#include "PDFPluginIdentifier.h"
#include "WebMouseEvent.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/ScrollableArea.h>
#include <wtf/HashMap.h>
#include <wtf/Identified.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

typedef struct objc_object* id;

OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSEvent;
OBJC_CLASS NSString;
OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFLayerController;
OBJC_CLASS PDFSelection;
OBJC_CLASS WKPDFLayerControllerDelegate;
OBJC_CLASS WKPDFPluginAccessibilityObject;

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;
typedef const struct OpaqueJSValue* JSValueRef;
typedef struct OpaqueJSClass* JSClassRef;

namespace WebCore {
class AXObjectCache;
class Element;
class FloatPoint;
class FloatSize;
class FragmentedSharedBuffer;
class GraphicsContext;
class HTMLPlugInElement;
class Scrollbar;
struct PluginInfo;
}

namespace WebKit {

class PDFPluginAnnotation;
class PDFPluginPasswordField;
class PluginView;
class ShareableBitmap;
class WebFrame;
class WebKeyboardEvent;
class WebWheelEvent;

struct FrameInfoData;
struct WebHitTestResultData;

class PDFPlugin final : public ThreadSafeRefCounted<PDFPlugin>, public WebCore::ScrollableArea {
public:
    static Ref<PDFPlugin> create(WebCore::HTMLPlugInElement&);
    ~PDFPlugin();

    bool isBeingDestroyed() const { return m_isBeingDestroyed; }

    static WebCore::PluginInfo pluginInfo();

    WebCore::IntSize size() const { return m_size; }

    void didMutatePDFDocument() { m_pdfDocumentWasMutated = true; }

    void paintControlForLayerInContext(CALayer *, CGContextRef);
    void setActiveAnnotation(PDFAnnotation *);

    void notifyContentScaleFactorChanged(CGFloat scaleFactor);
    void notifyDisplayModeChanged(int);

    void notifySelectionChanged(PDFSelection *);
    void notifyCursorChanged(uint64_t /* PDFLayerControllerCursorType */);

    void zoomIn();
    void zoomOut();
    void save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&);
    void openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&);
    PDFPluginIdentifier identifier() const { return m_identifier; }

    void clickedLink(NSURL *);

    void writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types);
    void showDefinitionForAttributedString(NSAttributedString *, CGPoint);
    void performWebSearch(NSString *);
    void performSpotlightSearch(NSString *);

    void focusNextAnnotation();
    void focusPreviousAnnotation();

    void attemptToUnlockPDF(const String& password);

    WebCore::FloatRect convertFromPDFViewToScreen(const WebCore::FloatRect&) const;
    WebCore::IntPoint convertFromRootViewToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntRect boundsOnScreen() const;

    bool showContextMenuAtPoint(const WebCore::IntPoint&);

    std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const;

    CGFloat scaleFactor() const;
    float deviceScaleFactor() const;

    PDFPluginAnnotation* activeAnnotation() const { return m_activeAnnotation.get(); }
    WebCore::AXObjectCache* axObjectCache() const;

#if HAVE(INCREMENTAL_PDF_APIS)
    void getResourceBytesAtPosition(size_t count, off_t position, CompletionHandler<void(const uint8_t*, size_t count)>&&);
    size_t getResourceBytesAtPositionMainThread(void* buffer, off_t position, size_t count);
    void receivedNonLinearizedPDFSentinel();
    bool incrementalPDFLoadingEnabled() const { return m_incrementalPDFLoadingEnabled; }
#endif

#if HAVE(INCREMENTAL_PDF_APIS) && !LOG_DISABLED
    void pdfLog(const String& event);
    size_t incrementThreadsWaitingOnCallback() { return ++m_threadsWaitingOnCallback; }
    size_t decrementThreadsWaitingOnCallback() { return --m_threadsWaitingOnCallback; }
#endif

    void setView(PluginView&);
    void destroy();
    void updateControlTints(WebCore::GraphicsContext&);
    RefPtr<ShareableBitmap> snapshot();
    CALayer *pluginLayer();
    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform);
    void contentsScaleFactorChanged(float);
    void visibilityDidChange(bool);
    void streamDidReceiveResponse(const WebCore::ResourceResponse&);
    void streamDidReceiveData(const WebCore::SharedBuffer&);
    void streamDidFinishLoading();
    void streamDidFail();
    bool handleMouseEvent(const WebMouseEvent&);
    bool handleWheelEvent(const WebWheelEvent&);
    bool handleMouseEnterEvent(const WebMouseEvent&);
    bool handleMouseLeaveEvent(const WebMouseEvent&);
    bool handleContextMenuEvent(const WebMouseEvent&);
    bool handleKeyboardEvent(const WebKeyboardEvent&);
    bool handleEditingCommand(StringView commandName);
    bool isEditingCommandEnabled(StringView commandName);
    WebCore::ScrollPosition scrollPositionForTesting() const { return scrollPosition(); }
    WebCore::Scrollbar* horizontalScrollbar() { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() { return m_verticalScrollbar.get(); }
    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const;
    void willDetachRenderer();

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const { return m_pdfDocument; }
    WebCore::FloatSize pdfDocumentSizeForPrinting() const;
    id accessibilityHitTest(const WebCore::IntPoint&) const;
    id accessibilityObject() const;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount);
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount);

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    String getSelectionString() const;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const;

    bool isFullFramePlugin() const;

private:
    explicit PDFPlugin(WebCore::HTMLPlugInElement&);

    PDFSelection *nextMatchForString(const String& target, bool searchForward, bool caseSensitive, bool wrapSearch, PDFSelection *initialSelection, bool startInSelection);

    // ScrollableArea functions.
    WebCore::IntRect scrollCornerRect() const final;
    WebCore::ScrollableArea* enclosingScrollableArea() const final;
    bool isScrollableOrRubberbandable() final { return true; }
    bool hasScrollableOrRubberbandableAncestor() final { return true; }
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const final;
    void setScrollOffset(const WebCore::ScrollOffset&) final;
    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) final;
    void invalidateScrollCornerRect(const WebCore::IntRect&) final;
    WebCore::IntPoint lastKnownMousePositionInView() const final { return m_lastMousePositionInPluginCoordinates; }
    bool isActive() const final;
    bool isScrollCornerVisible() const final { return false; }
    WebCore::ScrollPosition scrollPosition() const final;
    WebCore::ScrollPosition minimumScrollPosition() const final;
    WebCore::ScrollPosition maximumScrollPosition() const final;
    WebCore::IntSize visibleSize() const final { return m_size; }
    WebCore::IntSize contentsSize() const final { return m_pdfDocumentSize; }
    WebCore::Scrollbar* horizontalScrollbar() const final { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() const final { return m_verticalScrollbar.get(); }
    bool shouldSuspendScrollAnimations() const final { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) final;
    WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntRect& scrollbarRect) const final;
    WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntRect& parentRect) const final;
    WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntPoint& scrollbarPoint) const final;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntPoint& parentPoint) const final;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const final;
    bool shouldPlaceVerticalScrollbarOnLeft() const final { return false; }
    String debugDescription() const final;

    void updateScrollbars();
    Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    void destroyScrollbar(WebCore::ScrollbarOrientation);
    void installPDFDocument();
    void addArchiveResource();
    void calculateSizes();
    void tryRunScriptsInPDFDocument();

    NSEvent *nsEventForWebMouseEvent(const WebMouseEvent&);
    WebCore::IntPoint convertFromPluginToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromRootViewToPlugin(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromPDFViewToRootView(const WebCore::IntPoint&) const;
    WebCore::IntRect convertFromPDFViewToRootView(const WebCore::IntRect&) const;
    WebCore::IntRect frameForHUD() const;
    void ensureDataBufferLength(uint64_t length);

    bool supportsForms();

    bool handlesPageScaleFactor() const;
    void updatePageAndDeviceScaleFactors();

    void createPasswordEntryForm();

    WebCore::IntSize pdfDocumentSize() const { return m_pdfDocumentSize; }
    void setPDFDocumentSize(WebCore::IntSize size) { m_pdfDocumentSize = size; }

#ifdef __OBJC__
    NSData *liveData() const;
    NSData *rawData() const { return (__bridge NSData *)m_data.get(); }
#endif

    JSObjectRef makeJSPDFDoc(JSContextRef);
    static JSClassRef jsPDFDocClass();
    static JSValueRef jsPDFDocPrint(JSContextRef, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    WeakPtr<PluginView> m_view;
    WeakPtr<WebFrame> m_frame;

    bool m_pdfDocumentWasMutated { false };

    WebCore::IntSize m_scrollOffset;

    RetainPtr<CALayer> m_containerLayer;
    RetainPtr<CALayer> m_contentLayer;
    RetainPtr<CALayer> m_horizontalScrollbarLayer;
    RetainPtr<CALayer> m_verticalScrollbarLayer;
    RetainPtr<CALayer> m_scrollCornerLayer;
    RetainPtr<PDFLayerController> m_pdfLayerController;
    RetainPtr<WKPDFPluginAccessibilityObject> m_accessibilityObject;
    
    RefPtr<PDFPluginAnnotation> m_activeAnnotation;
    RefPtr<PDFPluginPasswordField> m_passwordField;
    RefPtr<WebCore::Element> m_annotationContainer;

    WebCore::AffineTransform m_rootViewToPluginTransform;
    WebMouseEvent m_lastMouseEvent;
    WebCore::IntPoint m_lastMousePositionInPluginCoordinates;

    String m_temporaryPDFUUID;

    String m_lastFoundString;

    RetainPtr<WKPDFLayerControllerDelegate> m_pdfLayerControllerDelegate;

    WebCore::IntSize m_size;

    URL m_sourceURL;

    String m_suggestedFilename;
    RetainPtr<CFMutableDataRef> m_data;
    uint64_t m_streamedBytes { 0 };

    RetainPtr<PDFDocument> m_pdfDocument;

    bool m_documentFinishedLoading { false };
    bool m_isBeingDestroyed { false };
    bool m_hasBeenDestroyed { false };
    unsigned m_firstPageHeight { 0 };
    WebCore::IntSize m_pdfDocumentSize; // All pages, including gaps.

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;

#if HAVE(INCREMENTAL_PDF_APIS)
    void threadEntry(Ref<PDFPlugin>&&);
    void adoptBackgroundThreadDocument();

    bool documentFinishedLoading() { return m_documentFinishedLoading; }
    uint64_t identifierForLoader(WebCore::NetscapePlugInStreamLoader* loader) { return m_streamLoaderMap.get(loader); }
    void removeOutstandingByteRangeRequest(uint64_t identifier) { m_outstandingByteRangeRequests.remove(identifier); }

    class PDFPluginStreamLoaderClient : public RefCounted<PDFPluginStreamLoaderClient>,
                                        public WebCore::NetscapePlugInStreamLoaderClient {
    public:
        PDFPluginStreamLoaderClient(PDFPlugin& pdfPlugin)
            : m_pdfPlugin(pdfPlugin)
        {
        }

        ~PDFPluginStreamLoaderClient() = default;

        void willSendRequest(WebCore::NetscapePlugInStreamLoader*, WebCore::ResourceRequest&&, const WebCore::ResourceResponse& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
        void didReceiveResponse(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceResponse&) final;
        void didReceiveData(WebCore::NetscapePlugInStreamLoader*, const WebCore::SharedBuffer&) final;
        void didFail(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceError&) final;
        void didFinishLoading(WebCore::NetscapePlugInStreamLoader*) final;

    private:
        WeakPtr<PDFPlugin> m_pdfPlugin;
    };

    class ByteRangeRequest : public Identified<ByteRangeRequest> {
    public:
        ByteRangeRequest() = default;
        ByteRangeRequest(uint64_t position, size_t count, CompletionHandler<void(const uint8_t*, size_t count)>&& completionHandler)
            : m_position(position)
            , m_count(count)
            , m_completionHandler(WTFMove(completionHandler))
        {
        }

        WebCore::NetscapePlugInStreamLoader* streamLoader() { return m_streamLoader; }
        void setStreamLoader(WebCore::NetscapePlugInStreamLoader* loader) { m_streamLoader = loader; }
        void clearStreamLoader();
        void addData(const uint8_t* data, size_t count) { m_accumulatedData.append(data, count); }

        void completeWithBytes(const uint8_t*, size_t, PDFPlugin&);
        void completeWithAccumulatedData(PDFPlugin&);

        bool maybeComplete(PDFPlugin&);
        void completeUnconditionally(PDFPlugin&);

        uint64_t position() const { return m_position; }
        size_t count() const { return m_count; }

    private:
        uint64_t m_position { 0 };
        size_t m_count { 0 };
        CompletionHandler<void(const uint8_t*, size_t count)> m_completionHandler;
        Vector<uint8_t> m_accumulatedData;
        WebCore::NetscapePlugInStreamLoader* m_streamLoader { nullptr };
    };
    void unconditionalCompleteOutstandingRangeRequests();

    ByteRangeRequest* byteRangeRequestForLoader(WebCore::NetscapePlugInStreamLoader&);
    void forgetLoader(WebCore::NetscapePlugInStreamLoader&);
    void cancelAndForgetLoader(WebCore::NetscapePlugInStreamLoader&);
    void maybeClearHighLatencyDataProviderFlag();

    RetainPtr<PDFDocument> m_backgroundThreadDocument;
    RefPtr<Thread> m_pdfThread;
    HashMap<uint64_t, ByteRangeRequest> m_outstandingByteRangeRequests;
    Ref<PDFPluginStreamLoaderClient> m_streamLoaderClient;
    HashMap<RefPtr<WebCore::NetscapePlugInStreamLoader>, uint64_t> m_streamLoaderMap;
    RangeSet<WTF::Range<uint64_t>> m_completedRanges;
    bool m_incrementalPDFLoadingEnabled;

#if !LOG_DISABLED
    void verboseLog();
    void logStreamLoader(TextStream&, WebCore::NetscapePlugInStreamLoader&);
    std::atomic<size_t> m_threadsWaitingOnCallback { 0 };
    std::atomic<size_t> m_completedRangeRequests { 0 };
    std::atomic<size_t> m_completedNetworkRangeRequests { 0 };
#endif

#endif // HAVE(INCREMENTAL_PDF_APIS)

    PDFPluginIdentifier m_identifier;
};

} // namespace WebKit

#endif // ENABLE(PDFKIT_PLUGIN)
