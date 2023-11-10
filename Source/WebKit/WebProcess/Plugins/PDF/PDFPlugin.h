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

#if ENABLE(LEGACY_PDFKIT_PLUGIN)

#include "PDFPluginBase.h"
#include "WebMouseEvent.h"
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <wtf/HashMap.h>
#include <wtf/Identified.h>
#include <wtf/Range.h>
#include <wtf/RangeSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

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
struct WebHitTestResultData;

class PDFPlugin final : public PDFPluginBase {
public:
    static bool pdfKitLayerControllerIsAvailable();

    static Ref<PDFPlugin> create(WebCore::HTMLPlugInElement&);
    virtual ~PDFPlugin() = default;

    void didMutatePDFDocument() { m_pdfDocumentWasMutated = true; }

    void paintControlForLayerInContext(CALayer *, CGContextRef);
    void setActiveAnnotation(PDFAnnotation *);

    void notifyContentScaleFactorChanged(CGFloat scaleFactor);
    void notifyDisplayModeChanged(int);

    void notifySelectionChanged(PDFSelection *);
    void notifyCursorChanged(uint64_t /* PDFLayerControllerCursorType */);

    // HUD Actions.
#if ENABLE(PDF_HUD)
    void zoomIn() final;
    void zoomOut() final;
    void save(CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&) final;
    void openWithPreview(CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&) final;
#endif

    void clickedLink(NSURL *);

    void writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types);
    void showDefinitionForAttributedString(NSAttributedString *, CGPoint);
    void performWebSearch(NSString *);
    void performSpotlightSearch(NSString *);

    void focusNextAnnotation();
    void focusPreviousAnnotation();

    void attemptToUnlockPDF(const String& password);

    bool showContextMenuAtPoint(const WebCore::IntPoint&);

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

private:
    explicit PDFPlugin(WebCore::HTMLPlugInElement&);
    bool isLegacyPDFPlugin() const override { return true; }

    PDFSelection *nextMatchForString(const String& target, bool searchForward, bool caseSensitive, bool wrapSearch, PDFSelection *initialSelection, bool startInSelection);

    void didChangeScrollOffset() override;

    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;
    void updateScrollbars() override;
    WebCore::IntPoint lastKnownMousePositionInView() const override { return m_lastMousePositionInPluginCoordinates; }
    Ref<Scrollbar> createScrollbar(WebCore::ScrollbarOrientation) override;
    void destroyScrollbar(WebCore::ScrollbarOrientation) override;

    // PDFPluginBase
    WebCore::PluginLayerHostingStrategy layerHostingStrategy() const override { return WebCore::PluginLayerHostingStrategy::PlatformLayer; }
    PlatformLayer* platformLayer() const override;

    void setView(PluginView&) override;
    void teardown() override;
    bool isComposited() const override { return true; }

    void installPDFDocument() override;
    void tryRunScriptsInPDFDocument() override;

    CGFloat scaleFactor() const override;

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const override { return m_pdfDocument; }
    WebCore::FloatSize pdfDocumentSizeForPrinting() const override;

    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    void contentsScaleFactorChanged(float) override;

    WebCore::IntSize contentsSize() const override;
    unsigned firstPageHeight() const override;

    RefPtr<WebCore::FragmentedSharedBuffer> liveResourceData() const override;

    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override;
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    bool handleEditingCommand(StringView commandName) override;
    bool isEditingCommandEnabled(StringView commandName) override;

    String getSelectionString() const override;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const override;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    std::tuple<String, PDFSelection *, NSDictionary *> lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&) const override;

    RefPtr<ShareableBitmap> snapshot() override;

    id accessibilityHitTest(const WebCore::IntPoint&) const override;
    id accessibilityObject() const override;
    id accessibilityAssociatedPluginParentForElement(WebCore::Element*) const override;

    void incrementalPDFStreamDidReceiveData(const WebCore::SharedBuffer&) override;
    bool incrementalPDFStreamDidFinishLoading() override;
    void incrementalPDFStreamDidFail() override;

    NSEvent *nsEventForWebMouseEvent(const WebMouseEvent&);

    bool supportsForms();

    bool handlesPageScaleFactor() const;
    void updatePageAndDeviceScaleFactors();

    void createPasswordEntryForm();

    NSData *liveData() const;
    JSObjectRef makeJSPDFDoc(JSContextRef);
    static JSClassRef jsPDFDocClass();
    static JSValueRef jsPDFDocPrint(JSContextRef, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);


    bool m_pdfDocumentWasMutated { false };

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

    std::optional<WebMouseEvent> m_lastMouseEvent;
    WebCore::IntPoint m_lastMousePositionInPluginCoordinates;

    String m_temporaryPDFUUID;
    String m_lastFoundString;

    RetainPtr<WKPDFLayerControllerDelegate> m_pdfLayerControllerDelegate;

    URL m_sourceURL;

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
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::PDFPlugin)
    static bool isType(const WebKit::PDFPluginBase& plugin) { return plugin.isLegacyPDFPlugin(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LEGACY_PDFKIT_PLUGIN)
