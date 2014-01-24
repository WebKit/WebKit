/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef PDFPlugin_h
#define PDFPlugin_h

#if ENABLE(PDFKIT_PLUGIN)

#include "Plugin.h"
#include "WebEvent.h"
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
OBJC_CLASS WKPDFLayerControllerDelegate;
OBJC_CLASS WKPDFPluginAccessibilityObject;
OBJC_CLASS WKPDFPluginContextMenuTarget;

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
    static PassRefPtr<PDFPlugin> create(WebFrame*);
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
    void openWithPlugin();
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

    void setUsedInPlaceOfBlockedPlugin(bool value, const String& useBlockedPluginContextMenuTitle);

private:
    explicit PDFPlugin(WebFrame*);

    // Plugin functions.
    virtual bool initialize(const Parameters&);
    virtual void destroy() override;
    virtual void paint(WebCore::GraphicsContext*, const WebCore::IntRect& dirtyRectInWindowCoordinates) override { }
    virtual void updateControlTints(WebCore::GraphicsContext*) override;
    virtual bool supportsSnapshotting() const override { return true; }
    virtual PassRefPtr<ShareableBitmap> snapshot() override;
    virtual PlatformLayer* pluginLayer() override;
    virtual bool isTransparent() override { return false; }
    virtual bool wantsWheelEvents() override { return true; }
    virtual void geometryDidChange(const WebCore::IntSize& pluginSize, const WebCore::IntRect& clipRect, const WebCore::AffineTransform& pluginToRootViewTransform) override;
    virtual void contentsScaleFactorChanged(float) override;
    virtual void visibilityDidChange() override { }
    virtual void frameDidFinishLoading(uint64_t requestID) override;
    virtual void frameDidFail(uint64_t requestID, bool wasCancelled) override;
    virtual void didEvaluateJavaScript(uint64_t requestID, const String& result) override;
    virtual void streamDidReceiveResponse(uint64_t streamID, const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const String& mimeType, const String& headers, const String& suggestedFileName) override;
    virtual void streamDidReceiveData(uint64_t streamID, const char* bytes, int length) override;
    virtual void streamDidFinishLoading(uint64_t streamID) override;
    virtual void streamDidFail(uint64_t streamID, bool wasCancelled) override;
    virtual void manualStreamDidReceiveResponse(const WebCore::URL& responseURL, uint32_t streamLength, uint32_t lastModifiedTime, const WTF::String& mimeType, const WTF::String& headers, const String& suggestedFileName) override;
    virtual void manualStreamDidReceiveData(const char* bytes, int length) override;
    virtual void manualStreamDidFinishLoading() override;
    virtual void manualStreamDidFail(bool wasCancelled) override;
    virtual bool handleMouseEvent(const WebMouseEvent&) override;
    virtual bool handleWheelEvent(const WebWheelEvent&) override;
    virtual bool handleMouseEnterEvent(const WebMouseEvent&) override;
    virtual bool handleMouseLeaveEvent(const WebMouseEvent&) override;
    virtual bool handleContextMenuEvent(const WebMouseEvent&) override;
    virtual bool handleKeyboardEvent(const WebKeyboardEvent&) override;
    virtual bool handleEditingCommand(const String& commandName, const String& argument) override;
    virtual bool isEditingCommandEnabled(const String&) override;
    virtual bool handlesPageScaleFactor() override;
    virtual void setFocus(bool) override { }
    virtual NPObject* pluginScriptableNPObject() override { return 0; }
    virtual void windowFocusChanged(bool) override { }
    virtual void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates) override { }
    virtual void windowVisibilityChanged(bool) override { }
    virtual uint64_t pluginComplexTextInputIdentifier() const override { return 0; }
    virtual void sendComplexTextInput(const String& textInput) override { }
    virtual void setLayerHostingMode(LayerHostingMode) override { }
    virtual WebCore::Scrollbar* horizontalScrollbar() override { return m_horizontalScrollbar.get(); }
    virtual WebCore::Scrollbar* verticalScrollbar() override { return m_verticalScrollbar.get(); }
    virtual void storageBlockingStateChanged(bool) override { }
    virtual void privateBrowsingStateChanged(bool) override { }
    virtual bool getFormValue(String& formValue) override { return false; }
    virtual bool handleScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity) override;
    virtual PassRefPtr<WebCore::SharedBuffer> liveResourceData() const override;

    virtual bool isBeingAsynchronouslyInitialized() const override { return false; }

    virtual RetainPtr<PDFDocument> pdfDocumentForPrinting() const override { return m_pdfDocument; }
    virtual NSObject *accessibilityObject() const override;

    virtual unsigned countFindMatches(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;
    virtual bool findString(const String& target, WebCore::FindOptions, unsigned maxMatchCount) override;

    PDFSelection *nextMatchForString(const String& target, BOOL searchForward, BOOL caseSensitive, BOOL wrapSearch, PDFSelection *initialSelection, BOOL startInSelection);

    virtual bool performDictionaryLookupAtLocation(const WebCore::FloatPoint&) override;
    virtual String getSelectionString() const override;

    virtual bool shouldAllowScripting() override { return false; }
    virtual bool shouldAllowNavigationFromDrags() override { return true; }
    virtual bool shouldAlwaysAutoStart() const override { return true; }

    // ScrollableArea functions.
    virtual WebCore::IntRect scrollCornerRect() const override;
    virtual WebCore::ScrollableArea* enclosingScrollableArea() const override;
    virtual WebCore::IntRect scrollableAreaBoundingBox() const override;
    virtual void setScrollOffset(const WebCore::IntPoint&) override;
    virtual void invalidateScrollbarRect(WebCore::Scrollbar*, const WebCore::IntRect&) override;
    virtual void invalidateScrollCornerRect(const WebCore::IntRect&) override;
    virtual WebCore::IntPoint lastKnownMousePosition() const override { return m_lastMousePositionInPluginCoordinates; }
    virtual int scrollSize(WebCore::ScrollbarOrientation) const override;
    virtual bool isActive() const override;
    virtual bool isScrollCornerVisible() const override { return false; }
    virtual int scrollPosition(WebCore::Scrollbar*) const override;
    virtual WebCore::IntPoint scrollPosition() const override;
    virtual WebCore::IntPoint minimumScrollPosition() const override;
    virtual WebCore::IntPoint maximumScrollPosition() const override;
    virtual WebCore::IntSize visibleSize() const override { return m_size; }
    virtual WebCore::IntSize contentsSize() const override { return m_pdfDocumentSize; }
    virtual WebCore::Scrollbar* horizontalScrollbar() const override { return m_horizontalScrollbar.get(); }
    virtual WebCore::Scrollbar* verticalScrollbar() const override { return m_verticalScrollbar.get(); }
    virtual bool shouldSuspendScrollAnimations() const override { return false; } // If we return true, ScrollAnimatorMac will keep cycling a timer forever, waiting for a good time to animate.
    virtual void scrollbarStyleChanged(int newStyle, bool forceUpdate) override;
    virtual WebCore::IntRect convertFromScrollbarToContainingView(const WebCore::Scrollbar*, const WebCore::IntRect& scrollbarRect) const override;
    virtual WebCore::IntRect convertFromContainingViewToScrollbar(const WebCore::Scrollbar*, const WebCore::IntRect& parentRect) const override;
    virtual WebCore::IntPoint convertFromScrollbarToContainingView(const WebCore::Scrollbar*, const WebCore::IntPoint& scrollbarPoint) const override;
    virtual WebCore::IntPoint convertFromContainingViewToScrollbar(const WebCore::Scrollbar*, const WebCore::IntPoint& parentPoint) const override;
    virtual bool updatesScrollLayerPositionOnMainThread() const override { return true; }

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
    bool isFullFramePlugin();

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
    bool m_usedInPlaceOfBlockedPlugin;

    String m_useBlockedPluginContextMenuTitle;

    WebCore::IntSize m_scrollOffset;

    RetainPtr<CALayer> m_containerLayer;
    RetainPtr<CALayer> m_contentLayer;
    RetainPtr<CALayer> m_horizontalScrollbarLayer;
    RetainPtr<CALayer> m_verticalScrollbarLayer;
    RetainPtr<CALayer> m_scrollCornerLayer;
    RetainPtr<PDFLayerController> m_pdfLayerController;
    RetainPtr<WKPDFPluginAccessibilityObject> m_accessibilityObject;
    RetainPtr<WKPDFPluginContextMenuTarget> m_contextMenuTarget;

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

#endif // ENABLE(PDFKIT_PLUGIN)

#endif // PDFPlugin_h
