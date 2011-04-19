/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WebPageProxy_h
#define WebPageProxy_h

#include "APIObject.h"
#include "Connection.h"
#include "ContextMenuState.h"
#include "DragControllerAction.h"
#include "DrawingAreaProxy.h"
#include "EditorState.h"
#include "GeolocationPermissionRequestManagerProxy.h"
#include "SandboxExtension.h"
#include "SharedMemory.h"
#include "WKBase.h"
#include "WKPagePrivate.h"
#include "WebContextMenuItemData.h"
#include "WebFindClient.h"
#include "WebFormClient.h"
#include "WebFrameProxy.h"
#include "WebHistoryClient.h"
#include "WebLoaderClient.h"
#include "WebPageContextMenuClient.h"
#include "WebPolicyClient.h"
#include "WebPopupMenuProxy.h"
#include "WebResourceLoadClient.h"
#include "WebUIClient.h"
#include <WebCore/ScrollTypes.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class AuthenticationChallenge;
    class Cursor;
    class DragData;
    class FloatRect;
    class IntSize;
    class ProtectionSpace;
    struct TextCheckingResult;
    struct ViewportArguments;
    struct WindowFeatures;
}

namespace WebKit {

class NativeWebKeyboardEvent;
class NativeWebMouseEvent;
class PageClient;
class PlatformCertificateInfo;
class StringPairVector;
class WebBackForwardList;
class WebBackForwardListItem;
class WebContextMenuProxy;
class WebData;
class WebEditCommandProxy;
class WebFullScreenManagerProxy;
class WebKeyboardEvent;
class WebMouseEvent;
class WebOpenPanelResultListenerProxy;
class WebPageGroup;
class WebProcessProxy;
class WebURLRequest;
class WebWheelEvent;
struct AttributedString;
struct DictionaryPopupInfo;
struct EditorState;
struct PlatformPopupMenuData;
struct PrintInfo;
struct WebPageCreationParameters;
struct WebPopupItem;

#if ENABLE(GESTURE_EVENTS)
class WebGestureEvent;
#endif

typedef GenericCallback<WKStringRef, StringImpl*> StringCallback;
typedef GenericCallback<WKSerializedScriptValueRef, WebSerializedScriptValue*> ScriptValueCallback;

// FIXME: Make a version of CallbackBase with three arguments, and define ValidateCommandCallback as a specialization.
class ValidateCommandCallback : public CallbackBase {
public:
    typedef void (*CallbackFunction)(WKStringRef, bool, int32_t, WKErrorRef, void*);

    static PassRefPtr<ValidateCommandCallback> create(void* context, CallbackFunction callback)
    {
        return adoptRef(new ValidateCommandCallback(context, callback));
    }

    virtual ~ValidateCommandCallback()
    {
        ASSERT(!m_callback);
    }

    void performCallbackWithReturnValue(StringImpl* returnValue1, bool returnValue2, int returnValue3)
    {
        ASSERT(m_callback);

        m_callback(toAPI(returnValue1), returnValue2, returnValue3, 0, context());

        m_callback = 0;
    }
    
    void invalidate()
    {
        ASSERT(m_callback);

        RefPtr<WebError> error = WebError::create();
        m_callback(0, 0, 0, toAPI(error.get()), context());
        
        m_callback = 0;
    }

private:

    ValidateCommandCallback(void* context, CallbackFunction callback)
        : CallbackBase(context)
        , m_callback(callback)
    {
    }

    CallbackFunction m_callback;
};

class WebPageProxy : public APIObject, public WebPopupMenuProxy::Client {
public:
    static const Type APIType = TypePage;

    static PassRefPtr<WebPageProxy> create(PageClient*, PassRefPtr<WebProcessProxy>, WebPageGroup*, uint64_t pageID);
    virtual ~WebPageProxy();

    uint64_t pageID() const { return m_pageID; }

    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    WebFrameProxy* focusedFrame() const { return m_focusedFrame.get(); }
    WebFrameProxy* frameSetLargestFrame() const { return m_frameSetLargestFrame.get(); }

    DrawingAreaProxy* drawingArea() { return m_drawingArea.get(); }
    void setDrawingArea(PassOwnPtr<DrawingAreaProxy>);

    WebBackForwardList* backForwardList() { return m_backForwardList.get(); }

#if ENABLE(INSPECTOR)
    WebInspectorProxy* inspector();
#endif

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxy* fullScreenManager();
#endif

    void initializeContextMenuClient(const WKPageContextMenuClient*);
    void initializeFindClient(const WKPageFindClient*);
    void initializeFormClient(const WKPageFormClient*);
    void initializeLoaderClient(const WKPageLoaderClient*);
    void initializePolicyClient(const WKPagePolicyClient*);
    void initializeResourceLoadClient(const WKPageResourceLoadClient*);
    void initializeUIClient(const WKPageUIClient*);

    void initializeWebPage();

    void close();
    bool tryClose();
    bool isClosed() const { return m_isClosed; }

    void loadURL(const String&);
    void loadURLRequest(WebURLRequest*);
    void loadHTMLString(const String& htmlString, const String& baseURL);
    void loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL);
    void loadPlainTextString(const String& string);

    void stopLoading();
    void reload(bool reloadFromOrigin);

    void goForward();
    bool canGoForward() const;
    void goBack();
    bool canGoBack() const;

    void goToBackForwardItem(WebBackForwardListItem*);
    void didChangeBackForwardList(WebBackForwardListItem* addedItem, Vector<RefPtr<APIObject> >* removedItems);
    void shouldGoToBackForwardListItem(uint64_t itemID, bool& shouldGoToBackForwardListItem);

    bool canShowMIMEType(const String& mimeType) const;

    bool drawsBackground() const { return m_drawsBackground; }
    void setDrawsBackground(bool);

    bool drawsTransparentBackground() const { return m_drawsTransparentBackground; }
    void setDrawsTransparentBackground(bool);

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void setInitialFocus(bool);
    void setWindowResizerSize(const WebCore::IntSize&);

    void setViewNeedsDisplay(const WebCore::IntRect&);
    void displayView();
    void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);

    enum {
        ViewWindowIsActive = 1 << 0,
        ViewIsFocused = 1 << 1,
        ViewIsVisible = 1 << 2,
        ViewIsInWindow = 1 << 3
    };
    typedef unsigned ViewStateFlags;
    void viewStateDidChange(ViewStateFlags flags);

    WebCore::IntSize viewSize() const;
    bool isViewVisible() const { return m_isVisible; }
    bool isViewWindowActive() const;

    void executeEditCommand(const String& commandName);
    void validateCommand(const String& commandName, PassRefPtr<ValidateCommandCallback>);

    const EditorState& editorState() const { return m_editorState; }
    bool canDelete() const { return hasSelectedRange() && isContentEditable(); }
    bool hasSelectedRange() const { return m_editorState.selectionIsRange; }
    bool isContentEditable() const { return m_editorState.isContentEditable; }

#if PLATFORM(MAC)
    void updateWindowIsVisible(bool windowIsVisible);
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates, const WebCore::IntPoint& accessibilityViewCoordinates);

    void setComposition(const String& text, Vector<WebCore::CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeEnd);
    void confirmComposition();
    bool insertText(const String& text, uint64_t replacementRangeStart, uint64_t replacementRangeEnd);
    void getMarkedRange(uint64_t& location, uint64_t& length);
    void getSelectedRange(uint64_t& location, uint64_t& length);
    void getAttributedSubstringFromRange(uint64_t location, uint64_t length, AttributedString&);
    uint64_t characterIndexForPoint(const WebCore::IntPoint);
    WebCore::IntRect firstRectForCharacterRange(uint64_t, uint64_t);
    bool executeKeypressCommands(const Vector<WebCore::KeypressCommand>&);

    void sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput);
    CGContextRef containingWindowGraphicsContext();
    bool shouldDelayWindowOrderingForEvent(const WebMouseEvent&);
    bool acceptsFirstMouse(int eventNumber, const WebMouseEvent&);
#endif
#if PLATFORM(WIN)
    void didChangeCompositionSelection(bool);
    void confirmComposition(const String&);
    void setComposition(const String&, Vector<WebCore::CompositionUnderline>&, int);
    WebCore::IntRect firstRectForCharacterInSelectedRange(int);
    String getSelectedText();

    bool gestureWillBegin(const WebCore::IntPoint&);
    void gestureDidScroll(const WebCore::IntSize&);
    void gestureDidEnd();

    void setGestureReachedScrollingLimit(bool);
#endif
#if ENABLE(TILED_BACKING_STORE)
    void setActualVisibleContentRect(const WebCore::IntRect& rect);
#endif

    void handleMouseEvent(const NativeWebMouseEvent&);
    void handleWheelEvent(const WebWheelEvent&);
    void handleKeyboardEvent(const NativeWebKeyboardEvent&);
#if ENABLE(GESTURE_EVENTS)
    void handleGestureEvent(const WebGestureEvent&);
#endif
#if ENABLE(TOUCH_EVENTS)
    void handleTouchEvent(const WebTouchEvent&);
#endif

    void scrollBy(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    String pageTitle() const;
    const String& toolTip() const { return m_toolTip; }

    void setUserAgent(const String&);
    const String& userAgent() const { return m_userAgent; }
    void setApplicationNameForUserAgent(const String&);
    const String& applicationNameForUserAgent() const { return m_applicationNameForUserAgent; }
    void setCustomUserAgent(const String&);
    const String& customUserAgent() const { return m_customUserAgent; }

    bool supportsTextEncoding() const;
    void setCustomTextEncodingName(const String&);
    String customTextEncodingName() const { return m_customTextEncodingName; }

    double estimatedProgress() const;

    void terminateProcess();

    typedef bool (*WebPageProxySessionStateFilterCallback)(WKPageRef, WKStringRef type, WKTypeRef object, void* context);
    PassRefPtr<WebData> sessionStateData(WebPageProxySessionStateFilterCallback, void* context) const;
    void restoreFromSessionStateData(WebData*);

    bool supportsTextZoom() const;
    double textZoomFactor() const { return m_mainFrameHasCustomRepresentation ? 1 : m_textZoomFactor; }
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);

    void scaleWebView(double scale, const WebCore::IntPoint& origin);
    double viewScaleFactor() const { return m_viewScaleFactor; }

    void setUseFixedLayout(bool);
    void setFixedLayoutSize(const WebCore::IntSize&);
    bool useFixedLayout() const { return m_useFixedLayout; };
    const WebCore::IntSize& fixedLayoutSize() const { return m_fixedLayoutSize; };

    bool hasHorizontalScrollbar() const { return m_mainFrameHasHorizontalScrollbar; }
    bool hasVerticalScrollbar() const { return m_mainFrameHasVerticalScrollbar; }

    bool isPinnedToLeftSide() const { return m_mainFrameIsPinnedToLeftSide; }
    bool isPinnedToRightSide() const { return m_mainFrameIsPinnedToRightSide; }

#if PLATFORM(MAC)
    // Called by the web process through a message.
    void registerWebProcessAccessibilityToken(const CoreIPC::DataReference&);
    // Called by the UI process when it is ready to send its tokens to the web process.
    void registerUIProcessAccessibilityTokens(const CoreIPC::DataReference& elemenToken, const CoreIPC::DataReference& windowToken);
    bool writeSelectionToPasteboard(const String& pasteboardName, const Vector<String>& pasteboardTypes);
    bool readSelectionFromPasteboard(const String& pasteboardName);
#endif

    void viewScaleFactorDidChange(double);

    void setMemoryCacheClientCallsEnabled(bool);

    // Find.
    void findString(const String&, FindOptions, unsigned maxMatchCount);
    void hideFindUI();
    void countStringMatches(const String&, FindOptions, unsigned maxMatchCount);
    void didCountStringMatches(const String&, uint32_t matchCount);
    void setFindIndicator(const WebCore::FloatRect& selectionRectInWindowCoordinates, const Vector<WebCore::FloatRect>& textRectsInSelectionRectCoordinates, const ShareableBitmap::Handle& contentImageHandle, bool fadeOut);
    void didFindString(const String&, uint32_t matchCount);
    void didFailToFindString(const String&);

    void getContentsAsString(PassRefPtr<StringCallback>);
    void getMainResourceDataOfFrame(WebFrameProxy*, PassRefPtr<DataCallback>);
    void getResourceDataFromFrame(WebFrameProxy*, WebURL*, PassRefPtr<DataCallback>);
    void getRenderTreeExternalRepresentation(PassRefPtr<StringCallback>);
    void getSelectionOrContentsAsString(PassRefPtr<StringCallback>);
    void getSourceForFrame(WebFrameProxy*, PassRefPtr<StringCallback>);
    void getWebArchiveOfFrame(WebFrameProxy*, PassRefPtr<DataCallback>);
    void runJavaScriptInMainFrame(const String&, PassRefPtr<ScriptValueCallback>);
    void forceRepaint(PassRefPtr<VoidCallback>);

    float headerHeight(WebFrameProxy*);
    float footerHeight(WebFrameProxy*);
    void drawHeader(WebFrameProxy*, const WebCore::FloatRect&);
    void drawFooter(WebFrameProxy*, const WebCore::FloatRect&);

#if PLATFORM(MAC)
    // Dictionary.
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
#endif

    void receivedPolicyDecision(WebCore::PolicyAction, WebFrameProxy*, uint64_t listenerID);

    void backForwardRemovedItem(uint64_t itemID);

    // Drag and drop support.
    void dragEntered(WebCore::DragData*, const String& dragStorageName = String());
    void dragUpdated(WebCore::DragData*, const String& dragStorageName = String());
    void dragExited(WebCore::DragData*, const String& dragStorageName = String());
    void performDrag(WebCore::DragData*, const String& dragStorageName, const SandboxExtension::Handle&);

    void didPerformDragControllerAction(uint64_t resultOperation);
    void dragEnded(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, uint64_t operation);
#if PLATFORM(MAC)
    void setDragImage(const WebCore::IntPoint& clientPosition, const ShareableBitmap::Handle& dragImageHandle, bool isLinkDrag);
#endif
#if PLATFORM(WIN)
    void startDragDrop(const WebCore::IntPoint& imagePoint, const WebCore::IntPoint& dragPoint, uint64_t okEffect, const HashMap<UINT, Vector<String> >& dataMap, const WebCore::IntSize& dragImageSize, const SharedMemory::Handle& dragImageHandle, bool isLinkDrag);
#endif
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void processDidBecomeUnresponsive();
    void processDidBecomeResponsive();
    void processDidCrash();

#if USE(ACCELERATED_COMPOSITING)
    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();
#endif
    
    void didDraw();

    enum UndoOrRedo { Undo, Redo };
    void addEditCommand(WebEditCommandProxy*);
    void removeEditCommand(WebEditCommandProxy*);
    bool isValidEditCommand(WebEditCommandProxy*);
    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo);

    WebProcessProxy* process() const;

    WebPageGroup* pageGroup() const { return m_pageGroup.get(); }

    bool isValid();
    
    WebCore::DragOperation dragOperation() { return m_currentDragOperation; }
    void resetDragOperation() { m_currentDragOperation = WebCore::DragOperationNone; }

    void preferencesDidChange();

#if ENABLE(TILED_BACKING_STORE)
    void setResizesToContentsUsingLayoutSize(const WebCore::IntSize&);
#endif

    // Called by the WebContextMenuProxy.
    void contextMenuItemSelected(const WebContextMenuItemData&);

    // Called by the WebOpenPanelResultListenerProxy.
    void didChooseFilesForOpenPanel(const Vector<String>&);
    void didCancelForOpenPanel();

    WebPageCreationParameters creationParameters() const;

#if PLATFORM(QT)
    void findZoomableAreaForPoint(const WebCore::IntPoint&);
#endif

    void advanceToNextMisspelling(bool startBeforeSelection) const;
    void changeSpellingToWord(const String& word) const;
#if PLATFORM(MAC)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();

    bool isSmartInsertDeleteEnabled() const { return m_isSmartInsertDeleteEnabled; }
    void setSmartInsertDeleteEnabled(bool);
#endif

    void beginPrinting(WebFrameProxy*, const PrintInfo&);
    void endPrinting();
    void computePagesForPrinting(WebFrameProxy*, const PrintInfo&, PassRefPtr<ComputedPagesCallback>);
#if PLATFORM(MAC) || PLATFORM(WIN)
    void drawRectToPDF(WebFrameProxy*, const WebCore::IntRect&, PassRefPtr<DataCallback>);
    void drawPagesToPDF(WebFrameProxy*, uint32_t first, uint32_t count, PassRefPtr<DataCallback>);
#endif

    const String& pendingAPIRequestURL() const { return m_pendingAPIRequestURL; }

    void flashBackingStoreUpdates(const Vector<WebCore::IntRect>& updateRects);

#if PLATFORM(MAC)
    void handleCorrectionPanelResult(const String& result);
#endif

    static void setDebugPaintFlags(WKPageDebugPaintFlags flags) { s_debugPaintFlags = flags; }
    static WKPageDebugPaintFlags debugPaintFlags() { return s_debugPaintFlags; }

    // Color to be used with kWKDebugFlashViewUpdates.
    static WebCore::Color viewUpdatesFlashColor();

    // Color to be used with kWKDebugFlashBackingStoreUpdates.
    static WebCore::Color backingStoreUpdatesFlashColor();

    void saveDataToFileInDownloadsFolder(const String& suggestedFilename, const String& mimeType, const String& originatingURLString, WebData*);

    void linkClicked(const String&, const WebMouseEvent&);
 
private:
    WebPageProxy(PageClient*, PassRefPtr<WebProcessProxy>, WebPageGroup*, uint64_t pageID);

    virtual Type type() const { return APIType; }

    // WebPopupMenuProxy::Client
    virtual void valueChangedForPopupMenu(WebPopupMenuProxy*, int32_t newSelectedIndex);
    virtual void setTextFromItemForPopupMenu(WebPopupMenuProxy*, int32_t index);
    virtual NativeWebMouseEvent* currentlyProcessedMouseDownEvent();

    // Implemented in generated WebPageProxyMessageReceiver.cpp
    void didReceiveWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void didCreateMainFrame(uint64_t frameID);
    void didCreateSubframe(uint64_t frameID, uint64_t parentFrameID);
    void didSaveFrameToPageCache(uint64_t frameID);
    void didRestoreFrameFromPageCache(uint64_t frameID, uint64_t parentFrameID);

    void didStartProvisionalLoadForFrame(uint64_t frameID, const String& url, const String& unreachableURL, CoreIPC::ArgumentDecoder*);
    void didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFailProvisionalLoadForFrame(uint64_t frameID, const WebCore::ResourceError&, CoreIPC::ArgumentDecoder*);
    void didCommitLoadForFrame(uint64_t frameID, const String& mimeType, bool frameHasCustomRepresentation, const PlatformCertificateInfo&, CoreIPC::ArgumentDecoder*);
    void didFinishDocumentLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFinishLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFailLoadForFrame(uint64_t frameID, const WebCore::ResourceError&, CoreIPC::ArgumentDecoder*);
    void didSameDocumentNavigationForFrame(uint64_t frameID, uint32_t sameDocumentNavigationType, const String&, CoreIPC::ArgumentDecoder*);
    void didReceiveTitleForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFirstLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didRemoveFrameFromHierarchy(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didDisplayInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didRunInsecureContentForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void frameDidBecomeFrameSet(uint64_t frameID, bool);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    
    void decidePolicyForNavigationAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const WebCore::ResourceRequest&, uint64_t listenerID, CoreIPC::ArgumentDecoder*, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID);
    void decidePolicyForNewWindowAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const WebCore::ResourceRequest&, const String& frameName, uint64_t listenerID, CoreIPC::ArgumentDecoder*);
    void decidePolicyForResponse(uint64_t frameID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, uint64_t listenerID, CoreIPC::ArgumentDecoder* arguments, bool& receivedPolicyAction, uint64_t& policyAction, uint64_t& downloadID);
    void unableToImplementPolicy(uint64_t frameID, const WebCore::ResourceError&, CoreIPC::ArgumentDecoder* arguments);

    void willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const StringPairVector& textFieldValues, uint64_t listenerID, CoreIPC::ArgumentDecoder*);

    // Resource load client
    void didInitiateLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const WebCore::ResourceRequest&, bool pageIsProvisionallyLoading);
    void didSendRequestForResource(uint64_t frameID, uint64_t resourceIdentifier, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse);
    void didReceiveResponseForResource(uint64_t frameID, uint64_t resourceIdentifier, const WebCore::ResourceResponse&);
    void didReceiveContentLengthForResource(uint64_t frameID, uint64_t resourceIdentifier, uint64_t contentLength);
    void didFinishLoadForResource(uint64_t frameID, uint64_t resourceIdentifier);
    void didFailLoadForResource(uint64_t frameID, uint64_t resourceIdentifier, const WebCore::ResourceError&);

    // UI client
    void createNewPage(const WebCore::WindowFeatures&, uint32_t modifiers, int32_t mouseButton, uint64_t& newPageID, WebPageCreationParameters&);
    void showPage();
    void closePage();
    void runJavaScriptAlert(uint64_t frameID, const String&);
    void runJavaScriptConfirm(uint64_t frameID, const String&, bool& result);
    void runJavaScriptPrompt(uint64_t frameID, const String&, const String&, String& result);
    void setStatusText(const String&);
    void mouseDidMoveOverElement(uint32_t modifiers, CoreIPC::ArgumentDecoder*);
    void missingPluginButtonClicked(const String& mimeType, const String& url, const String& pluginsPageURL);
    void setToolbarsAreVisible(bool toolbarsAreVisible);
    void getToolbarsAreVisible(bool& toolbarsAreVisible);
    void setMenuBarIsVisible(bool menuBarIsVisible);
    void getMenuBarIsVisible(bool& menuBarIsVisible);
    void setStatusBarIsVisible(bool statusBarIsVisible);
    void getStatusBarIsVisible(bool& statusBarIsVisible);
    void setIsResizable(bool isResizable);
    void getIsResizable(bool& isResizable);
    void setWindowFrame(const WebCore::FloatRect&);
    void getWindowFrame(WebCore::FloatRect&);
    void windowToScreen(const WebCore::IntRect& viewRect, WebCore::IntRect& result);
    void runBeforeUnloadConfirmPanel(const String& message, uint64_t frameID, bool& shouldClose);
    void didChangeViewportData(const WebCore::ViewportArguments&);
    void pageDidScroll();
    void runOpenPanel(uint64_t frameID, const WebOpenPanelParameters::Data&);
    void printFrame(uint64_t frameID);
    void exceededDatabaseQuota(uint64_t frameID, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentUsage, uint64_t expectedUsage, uint64_t& newQuota);
    void requestGeolocationPermissionForFrame(uint64_t geolocationID, uint64_t frameID, String originIdentifier);
    void runModal() { m_uiClient.runModal(this); }
    void didCompleteRubberBandForMainFrame(const WebCore::IntSize&);
    void didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar);
    void didChangeScrollOffsetPinningForMainFrame(bool pinnedToLeftSide, bool pinnedToRightSide);

    void reattachToWebProcess();
    void reattachToWebProcessWithItem(WebBackForwardListItem*);

#if ENABLE(TILED_BACKING_STORE)
    void pageDidRequestScroll(const WebCore::IntPoint&);
#endif

#if PLATFORM(QT)
    void didChangeContentsSize(const WebCore::IntSize&);
    void didFindZoomableArea(const WebCore::IntRect&);
#endif

    void editorStateChanged(const EditorState&);

    // Back/Forward list management
    void backForwardAddItem(uint64_t itemID);
    void backForwardGoToItem(uint64_t itemID);
    void backForwardItemAtIndex(int32_t index, uint64_t& itemID);
    void backForwardBackListCount(int32_t& count);
    void backForwardForwardListCount(int32_t& count);
    void backForwardClear();

    // Undo management
    void registerEditCommandForUndo(uint64_t commandID, uint32_t editAction);
    void clearAllEditCommands();
    void canUndoRedo(uint32_t action, bool& result);
    void executeUndoRedo(uint32_t action, bool& result);

    // Keyboard handling
#if PLATFORM(MAC)
    void interpretQueuedKeyEvent(const EditorState&, bool& handled, Vector<WebCore::KeypressCommand>&);
    void executeSavedCommandBySelector(const String& selector, bool& handled);
#endif

#if PLATFORM(GTK)
    void getEditorCommandsForKeyEvent(Vector<String>&);
#endif

    // Popup Menu.
    void showPopupMenu(const WebCore::IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData&);
    void hidePopupMenu();
#if PLATFORM(WIN)
    void setPopupMenuSelectedIndex(int32_t);
#endif

    // Context Menu.
    void showContextMenu(const WebCore::IntPoint& menuLocation, const ContextMenuState&, const Vector<WebContextMenuItemData>&, CoreIPC::ArgumentDecoder*);
    void internalShowContextMenu(const WebCore::IntPoint& menuLocation, const ContextMenuState&, const Vector<WebContextMenuItemData>&, CoreIPC::ArgumentDecoder*);

    // Search popup results
    void saveRecentSearches(const String&, const Vector<String>&);
    void loadRecentSearches(const String&, Vector<String>&);

#if PLATFORM(MAC)
    // Speech.
    void getIsSpeaking(bool&);
    void speak(const String&);
    void stopSpeaking();

    // Spotlight.
    void searchWithSpotlight(const String&);

    // Dictionary.
    void didPerformDictionaryLookup(const String&, const DictionaryPopupInfo&);
#endif

    // Spelling and grammar.
    int64_t spellDocumentTag();
#if USE(UNIFIED_TEXT_CHECKING)
    void checkTextOfParagraph(const String& text, uint64_t checkingTypes, Vector<WebCore::TextCheckingResult>& results);
#endif
    void checkSpellingOfString(const String& text, int32_t& misspellingLocation, int32_t& misspellingLength);
    void checkGrammarOfString(const String& text, Vector<WebCore::GrammarDetail>&, int32_t& badGrammarLocation, int32_t& badGrammarLength);
    void spellingUIIsShowing(bool&);
    void updateSpellingUIWithMisspelledWord(const String& misspelledWord);
    void updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const WebCore::GrammarDetail&);
    void getGuessesForWord(const String& word, const String& context, Vector<String>& guesses);
    void learnWord(const String& word);
    void ignoreWord(const String& word);

    void setFocus(bool focused);
    void takeFocus(uint32_t direction);
    void setToolTip(const String&);
    void setCursor(const WebCore::Cursor&);

    void didReceiveEvent(uint32_t opaqueType, bool handled);

    void voidCallback(uint64_t);
    void dataCallback(const CoreIPC::DataReference&, uint64_t);
    void stringCallback(const String&, uint64_t);
    void scriptValueCallback(const CoreIPC::DataReference&, uint64_t);
    void computedPagesCallback(const Vector<WebCore::IntRect>&, double totalScaleFactorForPrinting, uint64_t);
    void validateCommandCallback(const String&, bool, int, uint64_t);

    void focusedFrameChanged(uint64_t frameID);
    void frameSetLargestFrameChanged(uint64_t frameID);

    void canAuthenticateAgainstProtectionSpaceInFrame(uint64_t frameID, const WebCore::ProtectionSpace&, bool& canAuthenticate);
    void didReceiveAuthenticationChallenge(uint64_t frameID, const WebCore::AuthenticationChallenge&, uint64_t challengeID);

    void didFinishLoadingDataForCustomRepresentation(const String& suggestedFilename, const CoreIPC::DataReference&);

#if PLATFORM(MAC)
    void setComplexTextInputEnabled(uint64_t pluginComplexTextInputIdentifier, bool complexTextInputEnabled);
#endif

    static String standardUserAgent(const String& applicationName = String());

    void clearPendingAPIRequestURL() { m_pendingAPIRequestURL = String(); }
    void setPendingAPIRequestURL(const String& pendingAPIRequestURL) { m_pendingAPIRequestURL = pendingAPIRequestURL; }

    void initializeSandboxExtensionHandle(const WebCore::KURL&, SandboxExtension::Handle&);

#if PLATFORM(MAC)
    void substitutionsPanelIsShowing(bool&);
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    void showCorrectionPanel(int32_t panelType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings);
    void dismissCorrectionPanel(int32_t reason);
    void dismissCorrectionPanelSoon(int32_t reason, String& result);
    void recordAutocorrectionResponse(int32_t responseType, const String& replacedString, const String& replacementString);
#endif // !defined(BUILDING_ON_SNOW_LEOPARD)
#endif // PLATFORM(MAC)

    void clearLoadDependentCallbacks();

    void performDragControllerAction(DragControllerAction, WebCore::DragData*, const String& dragStorageName, const SandboxExtension::Handle&);

    PageClient* m_pageClient;
    WebLoaderClient m_loaderClient;
    WebPolicyClient m_policyClient;
    WebFormClient m_formClient;
    WebResourceLoadClient m_resourceLoadClient;
    WebUIClient m_uiClient;
    WebFindClient m_findClient;
    WebPageContextMenuClient m_contextMenuClient;

    OwnPtr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebProcessProxy> m_process;
    RefPtr<WebPageGroup> m_pageGroup;
    RefPtr<WebFrameProxy> m_mainFrame;
    RefPtr<WebFrameProxy> m_focusedFrame;
    RefPtr<WebFrameProxy> m_frameSetLargestFrame;

    String m_userAgent;
    String m_applicationNameForUserAgent;
    String m_customUserAgent;
    String m_customTextEncodingName;

#if ENABLE(INSPECTOR)
    RefPtr<WebInspectorProxy> m_inspector;
#endif

#if ENABLE(FULLSCREEN_API)
    RefPtr<WebFullScreenManagerProxy> m_fullScreenManager;
#endif

    HashMap<uint64_t, RefPtr<VoidCallback> > m_voidCallbacks;
    HashMap<uint64_t, RefPtr<DataCallback> > m_dataCallbacks;
    HashMap<uint64_t, RefPtr<StringCallback> > m_stringCallbacks;
    HashSet<uint64_t> m_loadDependentStringCallbackIDs;
    HashMap<uint64_t, RefPtr<ScriptValueCallback> > m_scriptValueCallbacks;
    HashMap<uint64_t, RefPtr<ComputedPagesCallback> > m_computedPagesCallbacks;
    HashMap<uint64_t, RefPtr<ValidateCommandCallback> > m_validateCommandCallbacks;

    HashSet<WebEditCommandProxy*> m_editCommandSet;

    RefPtr<WebPopupMenuProxy> m_activePopupMenu;
    RefPtr<WebContextMenuProxy> m_activeContextMenu;
    ContextMenuState m_activeContextMenuState;
    RefPtr<WebOpenPanelResultListenerProxy> m_openPanelResultListener;
    GeolocationPermissionRequestManagerProxy m_geolocationPermissionRequestManager;

    double m_estimatedProgress;

    // Whether the web page is contained in a top-level window.
    bool m_isInWindow;

    // Whether the page is visible; if the backing view is visible and inserted into a window.
    bool m_isVisible;

    bool m_canGoBack;
    bool m_canGoForward;
    RefPtr<WebBackForwardList> m_backForwardList;

    String m_toolTip;

    EditorState m_editorState;

    double m_textZoomFactor;
    double m_pageZoomFactor;
    double m_viewScaleFactor;

    bool m_drawsBackground;
    bool m_drawsTransparentBackground;

    bool m_areMemoryCacheClientCallsEnabled;

    bool m_useFixedLayout;
    WebCore::IntSize m_fixedLayoutSize;

    // If the process backing the web page is alive and kicking.
    bool m_isValid;

    // Whether WebPageProxy::close() has been called on this page.
    bool m_isClosed;

    bool m_isInPrintingMode;
    bool m_isPerformingDOMPrintOperation;

    bool m_inDecidePolicyForMIMEType;
    bool m_syncMimeTypePolicyActionIsValid;
    WebCore::PolicyAction m_syncMimeTypePolicyAction;
    uint64_t m_syncMimeTypePolicyDownloadID;

    bool m_inDecidePolicyForNavigationAction;
    bool m_syncNavigationActionPolicyActionIsValid;
    WebCore::PolicyAction m_syncNavigationActionPolicyAction;
    uint64_t m_syncNavigationActionPolicyDownloadID;

    Deque<NativeWebKeyboardEvent> m_keyEventQueue;
    bool m_processingWheelEvent;
    OwnPtr<WebWheelEvent> m_nextWheelEvent;

    bool m_processingMouseMoveEvent;
    OwnPtr<NativeWebMouseEvent> m_nextMouseMoveEvent;
    OwnPtr<NativeWebMouseEvent> m_currentlyProcessedMouseDownEvent;

    uint64_t m_pageID;

#if PLATFORM(MAC)
    bool m_isSmartInsertDeleteEnabled;
#endif

    int64_t m_spellDocumentTag;
    bool m_hasSpellDocumentTag;
    unsigned m_pendingLearnOrIgnoreWordMessageCount;

    bool m_mainFrameHasCustomRepresentation;
    WebCore::DragOperation m_currentDragOperation;

    String m_pendingAPIRequestURL;

    bool m_mainFrameHasHorizontalScrollbar;
    bool m_mainFrameHasVerticalScrollbar;

    bool m_mainFrameIsPinnedToLeftSide;
    bool m_mainFrameIsPinnedToRightSide;

    static WKPageDebugPaintFlags s_debugPaintFlags;
};

} // namespace WebKit

#endif // WebPageProxy_h
