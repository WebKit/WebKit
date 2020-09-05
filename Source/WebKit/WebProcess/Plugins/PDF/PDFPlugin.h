/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "PDFKitImports.h"
#include "PDFPluginIdentifier.h"
#include "Plugin.h"
#include "WebEvent.h"
#include "WebHitTestResultData.h"
#include <WebCore/AXObjectCache.h>
#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/ScrollableArea.h>
#include <wtf/HashMap.h>
#include <wtf/Identified.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/Threading.h>

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;
typedef const struct OpaqueJSValue* JSValueRef;

OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
OBJC_CLASS NSEvent;
OBJC_CLASS NSString;
OBJC_CLASS PDFAnnotation;
OBJC_CLASS PDFLayerController;
OBJC_CLASS PDFSelection;
OBJC_CLASS WKPDFPluginAccessibilityObject;
OBJC_CLASS WKPDFLayerControllerDelegate;

namespace IPC {
class DataReference;
}

namespace WebCore {
class AXObjectCache;
class Element;
struct PluginInfo;
}

namespace WTF {
class TextStream;
}

namespace WebKit {

class PDFPluginAnnotation;
class PDFPluginPasswordField;
class PluginView;
class WebFrame;
struct FrameInfoData;

class PDFPlugin final : public Plugin, public WebCore::ScrollableArea
#if HAVE(INCREMENTAL_PDF_APIS)
    , private WebCore::NetscapePlugInStreamLoaderClient
#endif
{
public:
    static Ref<PDFPlugin> create(WebFrame&);
    ~PDFPlugin();

    static WebCore::PluginInfo pluginInfo();

    WebCore::IntSize size() const { return m_size; }

    void didMutatePDFDocument() { m_pdfDocumentWasMutated = true; }

    void paintControlForLayerInContext(CALayer *, CGContextRef);
    void setActiveAnnotation(PDFAnnotation *);

    using ScrollableArea::notifyScrollPositionChanged;
    void notifyContentScaleFactorChanged(CGFloat scaleFactor);
    void notifyDisplayModeChanged(int);

    void notifySelectionChanged(PDFSelection *);
    void notifyCursorChanged(uint64_t /* PDFLayerControllerCursorType */);

#if ENABLE(UI_PROCESS_PDF_HUD)
    void zoomIn();
    void zoomOut();
    void save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&);
    void openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&);
    PDFPluginIdentifier identifier() const { return m_identifier; }
#endif

    void clickedLink(NSURL *);
#if !ENABLE(UI_PROCESS_PDF_HUD)
    void saveToPDF();
    void openWithNativeApplication();
#endif
    void writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types);
    void showDefinitionForAttributedString(NSAttributedString *, CGPoint);
    void performWebSearch(NSString *);
    void performSpotlightSearch(NSString *);

    void focusNextAnnotation();
    void focusPreviousAnnotation();

    void attemptToUnlockPDF(const String& password);

    WebCore::FloatRect convertFromPDFViewToScreen(const WebCore::FloatRect&) const;
    WebCore::IntPoint convertFromRootViewToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromPDFViewToRootView(const WebCore::IntPoint&) const;
    WebCore::IntRect convertFromPDFViewToRootView(const WebCore::IntRect&) const;
    WebCore::IntRect boundsOnScreen() const;
    WebCore::IntRect frameForHUD() const;

    bool showContextMenuAtPoint(const WebCore::IntPoint&);

    std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const;

    CGFloat scaleFactor() const;

    PDFPluginAnnotation* activeAnnotation() const { return m_activeAnnotation.get(); }
    WebCore::AXObjectCache* axObjectCache() const;

    void ensureDataBufferLength(uint64_t length);

#if HAVE(INCREMENTAL_PDF_APIS)
    void getResourceBytesAtPosition(size_t count, off_t position, CompletionHandler<void(const uint8_t*, size_t count)>&&);
    size_t getResourceBytesAtPositionMainThread(void* buffer, off_t position, size_t count);
    void receivedNonLinearizedPDFSentinel();
    bool incrementalPDFLoadingEnabled() const { return m_incrementalPDFLoadingEnabled; }
#ifndef NDEBUG
    void pdfLog(const String& event);
    size_t incrementThreadsWaitingOnCallback() { return ++m_threadsWaitingOnCallback; }
    size_t decrementThreadsWaitingOnCallback() { return --m_threadsWaitingOnCallback; }
#endif
#endif

private:
    explicit PDFPlugin(WebFrame&);

    // Plugin functions.
    bool initialize(const Parameters&) final;
    void destroy() final;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRectInWindowCoordinates) final { }
    void updateControlTints(WebCore::GraphicsContext&) final;
    bool supportsSnapshotting() const final { return true; }
    RefPtr<ShareableBitmap> snapshot() final;
    PlatformLayer* pluginLayer() final;
    bool isTransparent() final { return false; }
    bool wantsWheelEvents() final { return true; }
    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) final;
    void contentsScaleFactorChanged(float) final;
    void visibilityDidChange(bool) final;
    void frameDidFinishLoading(uint64_t requestID) final;
    void frameDidFail(uint64_t requestID, bool wasCancelled) final;
    void didEvaluateJavaScript(uint64_t requestID, const String& result) final;
    void streamWillSendRequest(uint64_t streamID, const URL& requestURL, const URL& responseURL, int responseStatus) final { }
    void streamDidReceiveResponse(uint64_t streamID, const URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) final;
    void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) final;
    void streamDidFinishLoading(uint64_t streamID) final;
    void streamDidFail(uint64_t streamID, bool wasCancelled) final;
    void manualStreamDidReceiveResponse(const URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& suggestedFileName) final;
    void manualStreamDidReceiveData(const char* bytes, int length) final;
    void manualStreamDidFinishLoading() final;
    void manualStreamDidFail(bool wasCancelled) final;
    bool handleMouseEvent(const WebMouseEvent&) final;
    bool handleWheelEvent(const WebWheelEvent&) final;
    bool handleMouseEnterEvent(const WebMouseEvent&) final;
    bool handleMouseLeaveEvent(const WebMouseEvent&) final;
    bool handleContextMenuEvent(const WebMouseEvent&) final;
    bool handleKeyboardEvent(const WebKeyboardEvent&) final;
    bool handleEditingCommand(const String& commandName, const String& argument) final;
    bool isEditingCommandEnabled(const String&) final;
    bool handlesPageScaleFactor() const final;
    bool requiresUnifiedScaleFactor() const final { return true; }
    void setFocus(bool) final { }
    NPObject* pluginScriptableNPObject() final { return nullptr; }
    void windowFocusChanged(bool) final { }
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) final { }
    void windowVisibilityChanged(bool) final { }
    uint64_t pluginComplexTextInputIdentifier() const final { return 0; }
    void sendComplexTextInput(const String& textInput) final { }
    void setLayerHostingMode(LayerHostingMode) final { }
    WebCore::Scrollbar* horizontalScrollbar() final { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() final { return m_verticalScrollbar.get(); }
    void storageBlockingStateChanged(bool) final { }
    void privateBrowsingStateChanged(bool) final { }
    bool getFormValue(String& formValue) final { return false; }
    bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) final;
    RefPtr<WebCore::SharedBuffer> liveResourceData() const final;
    void willDetachRenderer() final;
    bool pluginHandlesContentOffsetForAccessibilityHitTest() const final;
    
    bool isBeingAsynchronouslyInitialized() const final { return false; }

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const final { return m_pdfDocument; }
    NSObject *accessibilityObject() const final;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const final;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) final;
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) final;

    PDFSelection *nextMatchForString(const String& target, BOOL searchForward, BOOL caseSensitive, BOOL wrapSearch, PDFSelection *initialSelection, BOOL startInSelection);

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) final;
    String getSelectionString() const final;
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const final;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const final;

    bool shouldAllowScripting() final { return false; }
    bool shouldAllowNavigationFromDrags() final { return true; }
    bool shouldAlwaysAutoStart() const final { return true; }

    // ScrollableArea functions.
    bool isPDFPlugin() const final { return true; }
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
    bool shouldPlaceBlockDirectionScrollbarOnLeft() const final { return false; }
    String debugDescription() const final;

    // PDFPlugin functions.
    void updateScrollbars();
    Ref<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    void destroyScrollbar(WebCore::ScrollbarOrientation);
    void documentDataDidFinishLoading();
    void installPDFDocument();
    void addArchiveResource();
    void calculateSizes();
    void tryRunScriptsInPDFDocument();

    NSEvent *nsEventForWebMouseEvent(const WebMouseEvent&);
    WebCore::IntPoint convertFromPluginToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromRootViewToPlugin(const WebCore::IntPoint&) const;
    
    bool supportsForms();
    bool isFullFramePlugin() const;

    void updatePageAndDeviceScaleFactors();

    void createPasswordEntryForm();

    WebCore::IntSize pdfDocumentSize() const { return m_pdfDocumentSize; }
    void setPDFDocumentSize(WebCore::IntSize size) { m_pdfDocumentSize = size; }

#ifdef __OBJC__
    NSData *liveData() const;
    NSData *rawData() const { return (__bridge NSData *)m_data.get(); }
#endif

    JSObjectRef makeJSPDFDoc(JSContextRef);
    static JSValueRef jsPDFDocPrint(JSContextRef, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    void convertPostScriptDataIfNeeded();

    void setSuggestedFilename(const String&);

    // Regular plug-ins don't need access to view, but we add scrollbars to embedding FrameView for proper event handling.
    PluginView* pluginView();
    const PluginView* pluginView() const;

    WebFrame& m_frame;

    bool m_isPostScript { false };
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
    bool m_hasBeenDestroyed { false };
    unsigned m_firstPageHeight { 0 };
    WebCore::IntSize m_pdfDocumentSize; // All pages, including gaps.

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;

#if HAVE(INCREMENTAL_PDF_APIS)
    void threadEntry(Ref<PDFPlugin>&&);
    void adoptBackgroundThreadDocument();

    // WebCore::NetscapePlugInStreamLoaderClient
    void willSendRequest(WebCore::NetscapePlugInStreamLoader*, WebCore::ResourceRequest&&, const WebCore::ResourceResponse& redirectResponse, CompletionHandler<void(WebCore::ResourceRequest&&)>&&) final;
    void didReceiveResponse(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceResponse&) final;
    void didReceiveData(WebCore::NetscapePlugInStreamLoader*, const char*, int) final;
    void didFail(WebCore::NetscapePlugInStreamLoader*, const WebCore::ResourceError&) final;
    void didFinishLoading(WebCore::NetscapePlugInStreamLoader*) final;

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
    HashMap<RefPtr<WebCore::NetscapePlugInStreamLoader>, uint64_t> m_streamLoaderMap;
    RangeSet<WTF::Range<uint64_t>> m_completedRanges;
    bool m_incrementalPDFLoadingEnabled;

#if !LOG_DISABLED
    void verboseLog();
    void logStreamLoader(WTF::TextStream&, WebCore::NetscapePlugInStreamLoader&);
    std::atomic<size_t> m_threadsWaitingOnCallback { 0 };
    std::atomic<size_t> m_completedRangeRequests { 0 };
    std::atomic<size_t> m_completedNetworkRangeRequests { 0 };
#endif

#endif // HAVE(INCREMENTAL_PDF_APIS)
#if ENABLE(UI_PROCESS_PDF_HUD)
    PDFPluginIdentifier m_identifier;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::PDFPlugin)
    static bool isType(const WebKit::Plugin& plugin) { return plugin.isPDFPlugin(); }
    static bool isType(const WebCore::ScrollableArea& area) { return area.isPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(PDFKIT_PLUGIN)
