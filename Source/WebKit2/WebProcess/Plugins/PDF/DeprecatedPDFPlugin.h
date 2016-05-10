/*
 * Copyright (C) 2011, 2012, 2015 Apple Inc. All rights reserved.
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

#ifndef DeprecatedPDFPlugin_h
#define DeprecatedPDFPlugin_h

#include "PDFKitImports.h"

#if ENABLE(PDFKIT_PLUGIN) && USE(DEPRECATED_PDF_PLUGIN)

#include "Plugin.h"
#include "WebEvent.h"
#include "WebHitTestResultData.h"
#include <WebCore/AffineTransform.h>
#include <WebCore/FindOptions.h>
#include <WebCore/ScrollableArea.h>
#include <wtf/RetainPtr.h>

typedef const struct OpaqueJSContext* JSContextRef;
typedef struct OpaqueJSValue* JSObjectRef;
typedef const struct OpaqueJSValue* JSValueRef;

OBJC_CLASS NSArray;
OBJC_CLASS NSAttributedString;
OBJC_CLASS NSData;
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
class Element;
struct PluginInfo;
}

namespace WebKit {

class PDFPluginAnnotation;
class PDFPluginPasswordField;
class PluginView;
class WebFrame;

class PDFPlugin final : public Plugin, private WebCore::ScrollableArea {
public:
    static Ref<PDFPlugin> create(WebFrame*);
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

    void clickedLink(NSURL *);
    void saveToPDF();
    void openWithNativeApplication();
    void writeItemsToPasteboard(NSString *pasteboardName, NSArray *items, NSArray *types);
    void showDefinitionForAttributedString(NSAttributedString *, CGPoint);
    void performWebSearch(NSString *);
    void performSpotlightSearch(NSString *);

    void focusNextAnnotation();
    void focusPreviousAnnotation();

    void attemptToUnlockPDF(const String& password);

    WebCore::FloatRect convertFromPDFViewToScreen(const WebCore::FloatRect&) const;
    WebCore::IntRect boundsOnScreen() const;
    
    bool showContextMenuAtPoint(const WebCore::IntPoint&);

    String lookupTextAtLocation(const WebCore::FloatPoint&, WebHitTestResultData&, PDFSelection **, NSDictionary **) const;
    WebCore::FloatRect rectForSelectionInRootView(PDFSelection *) const;

    CGFloat scaleFactor() const;

    bool shouldPlaceBlockDirectionScrollbarOnLeft() const override { return false; }

private:
    explicit PDFPlugin(WebFrame*);

    // Plugin functions.
    bool initialize(const Parameters&) override;
    void destroy() override;
    void paint(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRectInWindowCoordinates) override { }
    void updateControlTints(WebCore::GraphicsContext&) override;
    bool supportsSnapshotting() const override { return true; }
    RefPtr<ShareableBitmap> snapshot() override;
    PlatformLayer* pluginLayer() override;
    bool isTransparent() override { return false; }
    bool wantsWheelEvents() override { return true; }
    void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    void contentsScaleFactorChanged(float) override;
    void visibilityDidChange(bool) override { }
    void frameDidFinishLoading(uint64_t requestID) override;
    void frameDidFail(uint64_t requestID, bool wasCancelled) override;
    void didEvaluateJavaScript(uint64_t requestID, const String& result) override;
    void streamWillSendRequest(uint64_t streamID, const WebCore::URL& requestURL, const WebCore::URL& responseURL, int responseStatus) override { }
    void streamDidReceiveResponse(uint64_t streamID, const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    void streamDidFinishLoading(uint64_t streamID) override;
    void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    void manualStreamDidReceiveResponse(const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& suggestedFileName) override;
    void manualStreamDidReceiveData(const char* bytes, int length) override;
    void manualStreamDidFinishLoading() override;
    void manualStreamDidFail(bool wasCancelled) override;
    bool handleMouseEvent(const WebMouseEvent&) override;
    bool handleWheelEvent(const WebWheelEvent&) override;
    bool handleMouseEnterEvent(const WebMouseEvent&) override;
    bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    bool handleContextMenuEvent(const WebMouseEvent&) override;
    bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    bool handleEditingCommand(const String& commandName, const String& argument) override;
    bool isEditingCommandEnabled(const String&) override;
    bool handlesPageScaleFactor() const override;
    bool requiresUnifiedScaleFactor() const override { return true; }
    void setFocus(bool) override { }
    NPObject* pluginScriptableNPObject() override { return 0; }
    void windowFocusChanged(bool) override { }
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override { }
    void windowVisibilityChanged(bool) override { }
    uint64_t pluginComplexTextInputIdentifier() const override { return 0; }
    void sendComplexTextInput(const String& textInput) override { }
    void setLayerHostingMode(LayerHostingMode) override { }
    WebCore::Scrollbar* horizontalScrollbar() override { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() override { return m_verticalScrollbar.get(); }
    void storageBlockingStateChanged(bool) override { }
    void privateBrowsingStateChanged(bool) override { }
    bool getFormValue(String& formValue) override { return false; }
    bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    RefPtr<WebCore::SharedBuffer> liveResourceData() const override;
    void willDetatchRenderer() override;

    bool isBeingAsynchronouslyInitialized() const override { return false; }

    RetainPtr<PDFDocument> pdfDocumentForPrinting() const override { return m_pdfDocument; }
    NSObject *accessibilityObject() const override;

    unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;

    PDFSelection *nextMatchForString(const String& target, BOOL searchForward, BOOL caseSensitive, BOOL wrapSearch, PDFSelection *initialSelection, BOOL startInSelection);

    bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    String getSelectionString() const override;
    String getSelectionForWordAtPoint(const WebCore::FloatPoint&) const override;
    bool existingSelectionContainsPoint(const WebCore::FloatPoint&) const override;

    bool shouldAllowScripting() override { return false; }
    bool shouldAllowNavigationFromDrags() override { return true; }
    bool shouldAlwaysAutoStart() const override { return true; }

    // ScrollableArea functions.
    WebCore::IntRect scrollCornerRect() const override;
    WebCore::ScrollableArea* enclosingScrollableArea() const override;
    bool isScrollableOrRubberbandable() override { return true; }
    bool hasScrollableOrRubberbandableAncestor() override { return true; }
    WebCore::IntRect scrollableAreaBoundingBox(bool* = nullptr) const override;
    void setScrollOffset(const WebCore::ScrollOffset&) override;
    void invalidateScrollbarRect(WebCore::Scrollbar&, const WebCore::IntRect&) override;
    void invalidateScrollCornerRect(const WebCore::IntRect&) override;
    WebCore::IntPoint lastKnownMousePosition() const override { return m_lastMousePositionInPluginCoordinates; }
    int scrollSize(WebCore::ScrollbarOrientation) const override;
    bool isActive() const override;
    bool isScrollCornerVisible() const override { return false; }
    int scrollOffset(WebCore::ScrollbarOrientation) const override;
    WebCore::ScrollPosition scrollPosition() const override;
    WebCore::ScrollPosition minimumScrollPosition() const override;
    WebCore::ScrollPosition maximumScrollPosition() const override;
    WebCore::IntSize visibleSize() const override { return m_size; }
    WebCore::IntSize contentsSize() const override { return m_pdfDocumentSize; }
    WebCore::Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    WebCore::Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    bool shouldSuspendScrollAnimations() const override { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    void scrollbarStyleChanged(WebCore::ScrollbarStyle, bool forceUpdate) override;
    WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntRect& scrollbarRect) const override;
    WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntRect& parentRect) const override;
    WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar&, const WebCore::IntPoint& scrollbarPoint) const override;
    WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar&, const WebCore::IntPoint& parentPoint) const override;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override;

    // PDFPlugin functions.
    void updateScrollbars();
    PassRefPtr<WebCore::Scrollbar> createScrollbar(WebCore::ScrollbarOrientation);
    void destroyScrollbar(WebCore::ScrollbarOrientation);
    void pdfDocumentDidLoad();
    void addArchiveResource();
    void computePageBoxes();
    void calculateSizes();
    void runScriptsInPDFDocument();

    NSEvent *nsEventForWebMouseEvent(const WebMouseEvent&);
    WebCore::IntPoint convertFromPluginToPDFView(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromRootViewToPlugin(const WebCore::IntPoint&) const;
    WebCore::IntPoint convertFromPDFViewToRootView(const WebCore::IntPoint&) const;
    
    bool supportsForms();
    bool isFullFramePlugin() const;

    void updatePageAndDeviceScaleFactors();

    void createPasswordEntryForm();

    RetainPtr<PDFDocument> pdfDocument() const { return m_pdfDocument; }
    void setPDFDocument(RetainPtr<PDFDocument> document) { m_pdfDocument = document; }

    WebCore::IntSize pdfDocumentSize() const { return m_pdfDocumentSize; }
    void setPDFDocumentSize(WebCore::IntSize size) { m_pdfDocumentSize = size; }

    NSData *liveData() const;
    NSData *rawData() const { return (NSData *)m_data.get(); }

    WebFrame* webFrame() const { return m_frame; }

    enum UpdateCursorMode {
        UpdateIfNeeded,
        ForceUpdate
    };

    enum HitTestResult {
        None,
        Text
    };

    void updateCursor(const WebMouseEvent&, UpdateCursorMode = UpdateIfNeeded);

    JSObjectRef makeJSPDFDoc(JSContextRef);
    static JSValueRef jsPDFDocPrint(JSContextRef, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

    void convertPostScriptDataIfNeeded();

    // Regular plug-ins don't need access to view, but we add scrollbars to embedding FrameView for proper event handling.
    PluginView* pluginView();
    const PluginView* pluginView() const;

    WebFrame* m_frame;

    bool m_isPostScript;
    bool m_pdfDocumentWasMutated;

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

    HitTestResult m_lastHitTestResult;
    
    RetainPtr<WKPDFLayerControllerDelegate> m_pdfLayerControllerDelegate;

    WebCore::IntSize m_size;

    WebCore::URL m_sourceURL;

    String m_suggestedFilename;
    RetainPtr<CFMutableDataRef> m_data;

    RetainPtr<PDFDocument> m_pdfDocument;
    Vector<WebCore::IntRect> m_pageBoxes;
    WebCore::IntSize m_pdfDocumentSize; // All pages, including gaps.

    RefPtr<WebCore::Scrollbar> m_horizontalScrollbar;
    RefPtr<WebCore::Scrollbar> m_verticalScrollbar;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_PLUGIN(PDFPlugin, PDFPluginType)

#endif // ENABLE(PDFKIT_PLUGIN)

#endif // DeprecatedPDFPlugin_h
