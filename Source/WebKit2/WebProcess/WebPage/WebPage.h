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

#ifndef WebPage_h
#define WebPage_h

#include "APIObject.h"
#include "DrawingArea.h"
#include "FindController.h"
#include "GeolocationPermissionRequestManager.h"
#include "ImageOptions.h"
#include "ImmutableArray.h"
#if ENABLE(CONTEXT_MENUS)
#include "InjectedBundlePageContextMenuClient.h"
#endif
#include "InjectedBundlePageEditorClient.h"
#include "InjectedBundlePageFormClient.h"
#include "InjectedBundlePageFullScreenClient.h"
#include "InjectedBundlePageLoaderClient.h"
#include "InjectedBundlePagePolicyClient.h"
#include "InjectedBundlePageResourceLoadClient.h"
#include "InjectedBundlePageUIClient.h"
#include "MessageSender.h"
#include "TapHighlightController.h"
#include "Plugin.h"
#include "SandboxExtension.h"
#include "ShareableBitmap.h"
#include "WebUndoStep.h"
#include <WebCore/DictationAlternative.h>
#include <WebCore/DragData.h>
#include <WebCore/Editor.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/WebCoreKeyboardUIMode.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(QT)
#include "ArgumentCodersQt.h"
#include "QtNetworkAccessManager.h"
#include "QtNetworkReply.h"
#include "QtNetworkReplyData.h"
#include "QtNetworkRequestData.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#include "WebPageAccessibilityObject.h"
#include "WebPrintOperationGtk.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include <WebCore/PlatformTouchEvent.h>
#endif

#if PLATFORM(MAC)
#include "DictionaryPopupInfo.h"
#include "LayerHostingContext.h"
#include <wtf/RetainPtr.h>
OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS WKAccessibilityWebPageObject;
#endif

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class GraphicsContext;
    class Frame;
    class FrameView;
    class HTMLPlugInElement;
    class KeyboardEvent;
    class Page;
    class PrintContext;
    class Range;
    class ResourceRequest;
    class SharedBuffer;
    class VisibleSelection;
    struct KeypressCommand;
}

namespace WebKit {

class DrawingArea;
class InjectedBundleBackForwardList;
class NotificationPermissionRequestManager;
class PageOverlay;
class PluginView;
class SessionState;
class WebContextMenu;
class WebContextMenuItemData;
class WebEvent;
class WebFrame;
class WebFullScreenManager;
class WebImage;
class WebInspector;
class WebKeyboardEvent;
class WebMouseEvent;
class WebNotificationClient;
class WebOpenPanelResultListener;
class WebPageGroupProxy;
class WebPopupMenu;
class WebWheelEvent;
struct AttributedString;
struct EditorState;
struct PrintInfo;
struct WebPageCreationParameters;
struct WebPreferencesStore;

#if ENABLE(GESTURE_EVENTS)
class WebGestureEvent;
#endif

#if ENABLE(TOUCH_EVENTS)
class WebTouchEvent;
#endif

class WebPage : public APIObject, public CoreIPC::MessageSender<WebPage> {
public:
    static const Type APIType = TypeBundlePage;

    static PassRefPtr<WebPage> create(uint64_t pageID, const WebPageCreationParameters&);
    virtual ~WebPage();

    // Used by MessageSender.
    CoreIPC::Connection* connection() const;
    uint64_t destinationID() const { return pageID(); }

    void close();

    WebCore::Page* corePage() const { return m_page.get(); }
    uint64_t pageID() const { return m_pageID; }

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }
    
    InjectedBundleBackForwardList* backForwardList();
    DrawingArea* drawingArea() const { return m_drawingArea.get(); }

    WebPageGroupProxy* pageGroup() const { return m_pageGroup.get(); }

    void scrollMainFrameIfNotAtMaxScrollPosition(const WebCore::IntSize& scrollOffset);

    void scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity);

    void centerSelectionInVisibleArea();

#if ENABLE(INSPECTOR)
    WebInspector* inspector();
#endif

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManager* fullScreenManager();
#endif

    // -- Called by the DrawingArea.
    // FIXME: We could genericize these into a DrawingArea client interface. Would that be beneficial?
    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect&);
    void drawPageOverlay(WebCore::GraphicsContext&, const WebCore::IntRect&);
    void layoutIfNeeded();

    // -- Called from WebCore clients.
#if PLATFORM(MAC)
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*, bool saveCommands);
#elif !PLATFORM(GTK)
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);
#endif

    void show();
    String userAgent() const { return m_userAgent; }
    WebCore::IntRect windowResizerRect() const;
    WebCore::KeyboardUIMode keyboardUIMode();

    WebUndoStep* webUndoStep(uint64_t);
    void addWebUndoStep(uint64_t, WebUndoStep*);
    void removeWebEditCommand(uint64_t);
    bool isInRedo() const { return m_isInRedo; }

    void setActivePopupMenu(WebPopupMenu*);
    
    WebOpenPanelResultListener* activeOpenPanelResultListener() const { return m_activeOpenPanelResultListener.get(); }
    void setActiveOpenPanelResultListener(PassRefPtr<WebOpenPanelResultListener>);

    // -- Called from WebProcess.
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);

    // -- InjectedBundle methods
#if ENABLE(CONTEXT_MENUS)
    void initializeInjectedBundleContextMenuClient(WKBundlePageContextMenuClient*);
#endif
    void initializeInjectedBundleEditorClient(WKBundlePageEditorClient*);
    void initializeInjectedBundleFormClient(WKBundlePageFormClient*);
    void initializeInjectedBundleLoaderClient(WKBundlePageLoaderClient*);
    void initializeInjectedBundlePolicyClient(WKBundlePagePolicyClient*);
    void initializeInjectedBundleResourceLoadClient(WKBundlePageResourceLoadClient*);
    void initializeInjectedBundleUIClient(WKBundlePageUIClient*);
#if ENABLE(FULLSCREEN_API)
    void initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClient*);
#endif

#if ENABLE(CONTEXT_MENUS)
    InjectedBundlePageContextMenuClient& injectedBundleContextMenuClient() { return m_contextMenuClient; }
#endif
    InjectedBundlePageEditorClient& injectedBundleEditorClient() { return m_editorClient; }
    InjectedBundlePageFormClient& injectedBundleFormClient() { return m_formClient; }
    InjectedBundlePageLoaderClient& injectedBundleLoaderClient() { return m_loaderClient; }
    InjectedBundlePagePolicyClient& injectedBundlePolicyClient() { return m_policyClient; }
    InjectedBundlePageResourceLoadClient& injectedBundleResourceLoadClient() { return m_resourceLoadClient; }
    InjectedBundlePageUIClient& injectedBundleUIClient() { return m_uiClient; }
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient& injectedBundleFullScreenClient() { return m_fullScreenClient; }
#endif

    void setUnderlayPage(PassRefPtr<WebPage> underlayPage) { m_underlayPage = underlayPage; }

    bool findStringFromInjectedBundle(const String&, FindOptions);

    WebFrame* mainWebFrame() const { return m_mainFrame.get(); }

    WebCore::Frame* mainFrame() const; // May return 0.
    WebCore::FrameView* mainFrameView() const; // May return 0.

    PassRefPtr<Plugin> createPlugin(WebFrame*, WebCore::HTMLPlugInElement*, const Plugin::Parameters&);

    EditorState editorState() const;

    String renderTreeExternalRepresentation() const;
    uint64_t renderTreeSize() const;
    void setPaintedObjectsCounterThreshold(uint64_t);

    void setTracksRepaints(bool);
    bool isTrackingRepaints() const;
    void resetTrackedRepaints();
    PassRefPtr<ImmutableArray> trackedRepaintRects();

    void executeEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    void clearMainFrameName();
    void sendClose();

    double textZoomFactor() const;
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);
    void windowScreenDidChange(uint64_t);

    void scalePage(double scale, const WebCore::IntPoint& origin);
    double pageScaleFactor() const;

    void setUseFixedLayout(bool);
    bool useFixedLayout() const { return m_useFixedLayout; }
    void setFixedLayoutSize(const WebCore::IntSize&);

    void setPaginationMode(uint32_t /* WebCore::Page::Pagination::Mode */);
    void setPaginationBehavesLikeColumns(bool);
    void setPageLength(double);
    void setGapBetweenPages(double);

    bool drawsBackground() const { return m_drawsBackground; }
    bool drawsTransparentBackground() const { return m_drawsTransparentBackground; }

    void stopLoading();
    void stopLoadingFrame(uint64_t frameID);
    void setDefersLoading(bool deferLoading);

#if USE(ACCELERATED_COMPOSITING)
    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();
#endif

    void addPluginView(PluginView*);
    void removePluginView(PluginView*);

#if PLATFORM(MAC)
    LayerHostingMode layerHostingMode() const { return m_layerHostingMode; }
    void setLayerHostingMode(LayerHostingMode);

    bool windowIsVisible() const { return m_windowIsVisible; }
    void updatePluginsActiveAndFocusedState();
    const WebCore::IntRect& windowFrameInScreenCoordinates() const { return m_windowFrameInScreenCoordinates; }
    const WebCore::IntRect& viewFrameInWindowCoordinates() const { return m_viewFrameInWindowCoordinates; }
#elif PLATFORM(WIN)
    HWND nativeWindow() const { return m_nativeWindow; }
#endif

    bool windowIsFocused() const;
    bool windowAndWebPageAreFocused() const;
    void installPageOverlay(PassRefPtr<PageOverlay>);
    void uninstallPageOverlay(PageOverlay*, bool fadeOut);
    bool hasPageOverlay() const { return m_pageOverlay; }
    WebCore::IntPoint screenToWindow(const WebCore::IntPoint&);
    WebCore::IntRect windowToScreen(const WebCore::IntRect&);

    PassRefPtr<WebImage> snapshotInViewCoordinates(const WebCore::IntRect&, ImageOptions);
    PassRefPtr<WebImage> snapshotInDocumentCoordinates(const WebCore::IntRect&, ImageOptions);
    PassRefPtr<WebImage> scaledSnapshotInDocumentCoordinates(const WebCore::IntRect&, double scaleFactor, ImageOptions);

    static const WebEvent* currentEvent();

    FindController& findController() { return m_findController; }
#if ENABLE(TOUCH_EVENTS) && PLATFORM(QT)
    TapHighlightController& tapHighlightController() { return m_tapHighlightController; }
#endif

#if ENABLE(GEOLOCATION)
    GeolocationPermissionRequestManager& geolocationPermissionRequestManager() { return m_geolocationPermissionRequestManager; }
#endif

    NotificationPermissionRequestManager* notificationPermissionRequestManager();

    void pageDidScroll();
#if USE(TILED_BACKING_STORE)
    void pageDidRequestScroll(const WebCore::IntPoint&);
    void setFixedVisibleContentRect(const WebCore::IntRect&);
    void setResizesToContentsUsingLayoutSize(const WebCore::IntSize&);
    void resizeToContentsIfNeeded();
    void sendViewportAttributesChanged();
    void setViewportSize(const WebCore::IntSize&);
    WebCore::IntSize viewportSize() const { return m_viewportSize; }
#endif

#if ENABLE(CONTEXT_MENUS)
    WebContextMenu* contextMenu();
#endif
    
    bool hasLocalDataForURL(const WebCore::KURL&);
    String cachedResponseMIMETypeForURL(const WebCore::KURL&);
    String cachedSuggestedFilenameForURL(const WebCore::KURL&);
    PassRefPtr<WebCore::SharedBuffer> cachedResponseDataForURL(const WebCore::KURL&);

    static bool canHandleRequest(const WebCore::ResourceRequest&);

    class SandboxExtensionTracker {
    public:
        ~SandboxExtensionTracker();

        void invalidate();

        void beginLoad(WebFrame*, const SandboxExtension::Handle& handle);
        void willPerformLoadDragDestinationAction(PassRefPtr<SandboxExtension> pendingDropSandboxExtension);
        void didStartProvisionalLoad(WebFrame*);
        void didCommitProvisionalLoad(WebFrame*);
        void didFailProvisionalLoad(WebFrame*);

    private:
        void setPendingProvisionalSandboxExtension(PassRefPtr<SandboxExtension>);

        RefPtr<SandboxExtension> m_pendingProvisionalSandboxExtension;
        RefPtr<SandboxExtension> m_provisionalSandboxExtension;
        RefPtr<SandboxExtension> m_committedSandboxExtension;
    };

    SandboxExtensionTracker& sandboxExtensionTracker() { return m_sandboxExtensionTracker; }

#if PLATFORM(QT)
    void setComposition(const String& text, Vector<WebCore::CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeEnd);
    void confirmComposition(const String& text, int64_t selectionStart, int64_t selectionLength);
    void cancelComposition();
#endif

#if PLATFORM(MAC)
    void registerUIProcessAccessibilityTokens(const CoreIPC::DataReference& elemenToken, const CoreIPC::DataReference& windowToken);
    WKAccessibilityWebPageObject* accessibilityRemoteObject();
    WebCore::IntPoint accessibilityPosition() const { return m_accessibilityPosition; }    
    
    void sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput);

    void setComposition(const String& text, Vector<WebCore::CompositionUnderline> underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeEnd, EditorState& newState);
    void confirmComposition(EditorState& newState);
    void cancelComposition(EditorState& newState);
    void insertText(const String& text, uint64_t replacementRangeStart, uint64_t replacementRangeEnd, bool& handled, EditorState& newState);
    void getMarkedRange(uint64_t& location, uint64_t& length);
    void getSelectedRange(uint64_t& location, uint64_t& length);
    void getAttributedSubstringFromRange(uint64_t location, uint64_t length, AttributedString&);
    void characterIndexForPoint(const WebCore::IntPoint point, uint64_t& result);
    void firstRectForCharacterRange(uint64_t location, uint64_t length, WebCore::IntRect& resultRect);
    void executeKeypressCommands(const Vector<WebCore::KeypressCommand>&, bool& handled, EditorState& newState);
    void readSelectionFromPasteboard(const WTF::String& pasteboardName, bool& result);
    void getStringSelectionForPasteboard(WTF::String& stringValue);
    void getDataSelectionForPasteboard(const WTF::String pasteboardType, SharedMemory::Handle& handle, uint64_t& size);
    void shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, bool& result);
    void acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent&, bool& result);
    bool performNonEditingBehaviorForSelector(const String&);
    void insertDictatedText(const String& text, uint64_t replacementRangeStart, uint64_t replacementRangeEnd, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool& handled, EditorState& newState);
#elif PLATFORM(WIN)
    void confirmComposition(const String& compositionString);
    void setComposition(const WTF::String& compositionString, const WTF::Vector<WebCore::CompositionUnderline>& underlines, uint64_t cursorPosition);
    void firstRectForCharacterInSelectedRange(const uint64_t characterPosition, WebCore::IntRect& resultRect);
    void getSelectedText(WTF::String&);

    void gestureWillBegin(const WebCore::IntPoint&, bool& canBeginPanning);
    void gestureDidScroll(const WebCore::IntSize&);
    void gestureDidEnd();

#elif PLATFORM(GTK)
    void updateAccessibilityTree();
#endif

    void setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length);
    bool hasCompositionForTesting();
    void confirmCompositionForTesting(const String& compositionString);

    // FIXME: This a dummy message, to avoid breaking the build for platforms that don't require
    // any synchronous messages, and should be removed when <rdar://problem/8775115> is fixed.
    void dummy(bool&);

#if PLATFORM(MAC)
    void performDictionaryLookupForSelection(DictionaryPopupInfo::Type, WebCore::Frame*, const WebCore::VisibleSelection&);

    bool isSpeaking();
    void speak(const String&);
    void stopSpeaking();

    bool isSmartInsertDeleteEnabled() const { return m_isSmartInsertDeleteEnabled; }
#endif

    void replaceSelectionWithText(WebCore::Frame*, const String&);
    void clearSelection();
#if PLATFORM(WIN)
    void performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const WebCore::DragDataMap&, uint32_t flags);
#elif PLATFORM(QT) || PLATFORM(GTK)
    void performDragControllerAction(uint64_t action, WebCore::DragData);
#else
    void performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const WTF::String& dragStorageName, uint32_t flags, const SandboxExtension::Handle&, const SandboxExtension::HandleArray&);
#endif
    void dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t operation);

    void willPerformLoadDragDestinationAction();
    void mayPerformUploadDragDestinationAction();

    void beginPrinting(uint64_t frameID, const PrintInfo&);
    void endPrinting();
    void computePagesForPrinting(uint64_t frameID, const PrintInfo&, uint64_t callbackID);
#if PLATFORM(MAC) || PLATFORM(WIN)
    void drawRectToPDF(uint64_t frameID, const PrintInfo&, const WebCore::IntRect&, uint64_t callbackID);
    void drawPagesToPDF(uint64_t frameID, const PrintInfo&, uint32_t first, uint32_t count, uint64_t callbackID);
#elif PLATFORM(GTK)
    void drawPagesForPrinting(uint64_t frameID, const PrintInfo&, uint64_t callbackID);
#endif

    void setMediaVolume(float);

    bool mainFrameHasCustomRepresentation() const;

    void didChangeScrollOffsetForMainFrame();

    void mainFrameDidLayout();

    bool canRunBeforeUnloadConfirmPanel() const { return m_canRunBeforeUnloadConfirmPanel; }
    void setCanRunBeforeUnloadConfirmPanel(bool canRunBeforeUnloadConfirmPanel) { m_canRunBeforeUnloadConfirmPanel = canRunBeforeUnloadConfirmPanel; }

    bool canRunModal() const { return m_canRunModal; }
    void setCanRunModal(bool canRunModal) { m_canRunModal = canRunModal; }

    void runModal();

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const;

    void setMemoryCacheMessagesEnabled(bool);

    void forceRepaintWithoutCallback();

    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();
#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD)
    void handleAlternativeTextUIResult(const String&);
#endif

    // For testing purpose.
    void simulateMouseDown(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, double time);
    void simulateMouseUp(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, double time);
    void simulateMouseMotion(WebCore::IntPoint, double time);
    String viewportConfigurationAsText(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight);

#if ENABLE(CONTEXT_MENUS)
    void contextMenuShowing() { m_isShowingContextMenu = true; }
#endif

#if PLATFORM(QT)
    void registerApplicationScheme(const String& scheme);
    void applicationSchemeReply(const QtNetworkReplyData&);
    void receivedApplicationSchemeRequest(const QNetworkRequest&, QtNetworkReply*);
#endif
    void wheelEvent(const WebWheelEvent&);
#if ENABLE(GESTURE_EVENTS)
    void gestureEvent(const WebGestureEvent&);
#endif

    void numWheelEventHandlersChanged(unsigned);
    void recomputeShortCircuitHorizontalWheelEventsState();

    bool willGoToBackForwardItemCallbackEnabled() const { return m_willGoToBackForwardItemCallbackEnabled; }

#if ENABLE(PAGE_VISIBILITY_API)
    void setVisibilityState(int visibilityState, bool isInitialState);
#endif

private:
    WebPage(uint64_t pageID, const WebPageCreationParameters&);

    virtual Type type() const { return APIType; }

    void platformInitialize();

    void didReceiveWebPageMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncWebPageMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);

#if !PLATFORM(MAC)
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
#endif
    bool performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&);

#if PLATFORM(MAC)
    bool executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>&, WebCore::KeyboardEvent*);
#endif

    String sourceForFrame(WebFrame*);

    void loadData(PassRefPtr<WebCore::SharedBuffer>, const String& MIMEType, const String& encodingName, const WebCore::KURL& baseURL, const WebCore::KURL& failingURL);

    bool platformHasLocalDataForURL(const WebCore::KURL&);

    // Actions
    void tryClose();
    void loadURL(const String&, const SandboxExtension::Handle&);
    void loadURLRequest(const WebCore::ResourceRequest&, const SandboxExtension::Handle&);
    void loadHTMLString(const String& htmlString, const String& baseURL);
    void loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL);
    void loadPlainTextString(const String&);
    void loadWebArchiveData(const CoreIPC::DataReference&);
    void linkClicked(const String& url, const WebMouseEvent&);
    void reload(bool reloadFromOrigin, const SandboxExtension::Handle&);
    void goForward(uint64_t);
    void goBack(uint64_t);
    void goToBackForwardItem(uint64_t);
    void tryRestoreScrollPosition();
    void setActive(bool);
    void setFocused(bool);
    void setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent&);
    void setWindowResizerSize(const WebCore::IntSize&);
    void setIsInWindow(bool);
    void validateCommand(const String&, uint64_t);
    void executeEditCommand(const String&);

    void mouseEvent(const WebMouseEvent&);
    void mouseEventSyncForTesting(const WebMouseEvent&, bool&);
    void wheelEventSyncForTesting(const WebWheelEvent&, bool&);
    void keyEvent(const WebKeyboardEvent&);
    void keyEventSyncForTesting(const WebKeyboardEvent&, bool&);
#if ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
    void touchEventSyncForTesting(const WebTouchEvent&, bool& handled);
#if PLATFORM(QT)
    void highlightPotentialActivation(const WebCore::IntPoint&, const WebCore::IntSize& area);
#endif
#endif
#if ENABLE(CONTEXT_MENUS)
    void contextMenuHidden() { m_isShowingContextMenu = false; }
#endif

    static void scroll(WebCore::Page*, WebCore::ScrollDirection, WebCore::ScrollGranularity);
    static void logicalScroll(WebCore::Page*, WebCore::ScrollLogicalDirection, WebCore::ScrollGranularity);

    uint64_t restoreSession(const SessionState&);
    void restoreSessionAndNavigateToCurrentItem(const SessionState&);

    void didRemoveBackForwardItem(uint64_t);

    void setWillGoToBackForwardItemCallbackEnabled(bool enabled) { m_willGoToBackForwardItemCallbackEnabled = enabled; }
    
    void setDrawsBackground(bool);
    void setDrawsTransparentBackground(bool);

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void getContentsAsString(uint64_t callbackID);
    void getMainResourceDataOfFrame(uint64_t frameID, uint64_t callbackID);
    void getResourceDataFromFrame(uint64_t frameID, const String& resourceURL, uint64_t callbackID);
    void getRenderTreeExternalRepresentation(uint64_t callbackID);
    void getSelectionOrContentsAsString(uint64_t callbackID);
    void getSourceForFrame(uint64_t frameID, uint64_t callbackID);
    void getWebArchiveOfFrame(uint64_t frameID, uint64_t callbackID);
    void runJavaScriptInMainFrame(const String&, uint64_t callbackID);
    void forceRepaint(uint64_t callbackID);

    void preferencesDidChange(const WebPreferencesStore&);
    void platformPreferencesDidChange(const WebPreferencesStore&);
    void updatePreferences(const WebPreferencesStore&);

    void didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction, uint64_t downloadID);
    void setUserAgent(const String&);
    void setCustomTextEncodingName(const String&);
    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

#if PLATFORM(MAC)
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    void performDictionaryLookupForRange(DictionaryPopupInfo::Type, WebCore::Frame*, WebCore::Range*, NSDictionary *options);

    void setWindowIsVisible(bool windowIsVisible);
    void windowAndViewFramesChanged(const WebCore::IntRect& windowFrameInScreenCoordinates, const WebCore::IntRect& viewFrameInWindowCoordinates, const WebCore::IntPoint& accessibilityViewCoordinates);

    RetainPtr<PDFDocument> pdfDocumentForPrintingFrame(WebCore::Frame*);
    void computePagesForPrintingPDFDocument(uint64_t frameID, const PrintInfo&, Vector<WebCore::IntRect>& resultPageRects);
    void drawRectToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, const WebCore::IntRect&);
    void drawPagesToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, uint32_t first, uint32_t count);
#endif

    void unapplyEditCommand(uint64_t commandID);
    void reapplyEditCommand(uint64_t commandID);
    void didRemoveEditCommand(uint64_t commandID);

    void findString(const String&, uint32_t findOptions, uint32_t maxMatchCount);
    void hideFindUI();
    void countStringMatches(const String&, uint32_t findOptions, uint32_t maxMatchCount);

#if PLATFORM(QT)
    void findZoomableAreaForPoint(const WebCore::IntPoint&, const WebCore::IntSize& area);
#endif

    void didChangeSelectedIndexForActivePopupMenu(int32_t newIndex);
    void setTextForActivePopupMenu(int32_t index);

#if PLATFORM(GTK)
    void failedToShowPopupMenu();
#endif

    void didChooseFilesForOpenPanel(const Vector<String>&);
    void didCancelForOpenPanel();
#if ENABLE(WEB_PROCESS_SANDBOX)
    void extendSandboxForFileFromOpenPanel(const SandboxExtension::Handle&);
#endif

    void didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed);

    void didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed);

    void advanceToNextMisspelling(bool startBeforeSelection);
    void changeSpellingToWord(const String& word);
#if PLATFORM(MAC)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();

    void setSmartInsertDeleteEnabled(bool isSmartInsertDeleteEnabled) { m_isSmartInsertDeleteEnabled = isSmartInsertDeleteEnabled; }
#endif

#if ENABLE(CONTEXT_MENUS)
    void didSelectItemFromActiveContextMenu(const WebContextMenuItemData&);
#endif

    void setCanStartMediaTimerFired();

    static bool platformCanHandleRequest(const WebCore::ResourceRequest&);

    OwnPtr<WebCore::Page> m_page;
    RefPtr<WebFrame> m_mainFrame;
    RefPtr<InjectedBundleBackForwardList> m_backForwardList;

    RefPtr<WebPageGroupProxy> m_pageGroup;

    String m_userAgent;

    WebCore::IntSize m_viewSize;
    OwnPtr<DrawingArea> m_drawingArea;

    HashSet<PluginView*> m_pluginViews;

    bool m_useFixedLayout;

    bool m_drawsBackground;
    bool m_drawsTransparentBackground;

    bool m_isInRedo;
    bool m_isClosed;

    bool m_tabToLinks;

#if PLATFORM(MAC)
    // Whether the containing window is visible or not.
    bool m_windowIsVisible;

    // Whether smart insert/delete is enabled or not.
    bool m_isSmartInsertDeleteEnabled;

    // The frame of the containing window in screen coordinates.
    WebCore::IntRect m_windowFrameInScreenCoordinates;

    // The frame of the view in window coordinates.
    WebCore::IntRect m_viewFrameInWindowCoordinates;

    // The accessibility position of the view.
    WebCore::IntPoint m_accessibilityPosition;
    
    // The layer hosting mode.
    LayerHostingMode m_layerHostingMode;

    RetainPtr<WKAccessibilityWebPageObject> m_mockAccessibilityElement;

    WebCore::KeyboardEvent* m_keyboardEventBeingInterpreted;

#elif PLATFORM(WIN)
    // Our view's window (in the UI process).
    HWND m_nativeWindow;

    RefPtr<WebCore::Node> m_gestureTargetNode;
#elif PLATFORM(GTK)
    WebPageAccessibilityObject* m_accessibilityObject;
#endif
    
    WebCore::RunLoop::Timer<WebPage> m_setCanStartMediaTimer;

    HashMap<uint64_t, RefPtr<WebUndoStep> > m_undoStepMap;

    WebCore::IntSize m_windowResizerSize;

#if ENABLE(CONTEXT_MENUS)
    InjectedBundlePageContextMenuClient m_contextMenuClient;
#endif
    InjectedBundlePageEditorClient m_editorClient;
    InjectedBundlePageFormClient m_formClient;
    InjectedBundlePageLoaderClient m_loaderClient;
    InjectedBundlePagePolicyClient m_policyClient;
    InjectedBundlePageResourceLoadClient m_resourceLoadClient;
    InjectedBundlePageUIClient m_uiClient;
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient m_fullScreenClient;
#endif

#if USE(TILED_BACKING_STORE)
    WebCore::IntSize m_viewportSize;
#endif

    FindController m_findController;
#if ENABLE(TOUCH_EVENTS) && PLATFORM(QT)
    TapHighlightController m_tapHighlightController;
#endif
    RefPtr<PageOverlay> m_pageOverlay;

    RefPtr<WebPage> m_underlayPage;

#if ENABLE(INSPECTOR)
    RefPtr<WebInspector> m_inspector;
#endif
#if ENABLE(FULLSCREEN_API)
    RefPtr<WebFullScreenManager> m_fullScreenManager;
#endif
    RefPtr<WebPopupMenu> m_activePopupMenu;
#if ENABLE(CONTEXT_MENUS)
    RefPtr<WebContextMenu> m_contextMenu;
#endif
    RefPtr<WebOpenPanelResultListener> m_activeOpenPanelResultListener;
    RefPtr<NotificationPermissionRequestManager> m_notificationPermissionRequestManager;

#if ENABLE(GEOLOCATION)
    GeolocationPermissionRequestManager m_geolocationPermissionRequestManager;
#endif

    OwnPtr<WebCore::PrintContext> m_printContext;
#if PLATFORM(GTK)
    RefPtr<WebPrintOperationGtk> m_printOperation;
#endif

    SandboxExtensionTracker m_sandboxExtensionTracker;
    uint64_t m_pageID;

    RefPtr<SandboxExtension> m_pendingDropSandboxExtension;
    Vector<RefPtr<SandboxExtension> > m_pendingDropExtensionsForFileUpload;

    bool m_canRunBeforeUnloadConfirmPanel;

    bool m_canRunModal;
    bool m_isRunningModal;

    bool m_cachedMainFrameIsPinnedToLeftSide;
    bool m_cachedMainFrameIsPinnedToRightSide;
    bool m_canShortCircuitHorizontalWheelEvents;
    unsigned m_numWheelEventHandlers;

    unsigned m_cachedPageCount;

#if ENABLE(CONTEXT_MENUS)
    bool m_isShowingContextMenu;
#endif
    
    bool m_willGoToBackForwardItemCallbackEnabled;

#if PLATFORM(WIN)
    bool m_gestureReachedScrollingLimit;
#endif
#if PLATFORM(QT)
    HashMap<String, QtNetworkReply*> m_applicationSchemeReplies;
#endif
};

} // namespace WebKit

#endif // WebPage_h
