/*
 * Copyright (C) 2010, 2011, 2013 Apple Inc. All rights reserved.
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

#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "APIObject.h"
#include "DictionaryPopupInfo.h"
#include "FindController.h"
#include "GeolocationPermissionRequestManager.h"
#include "ImageOptions.h"
#include "InjectedBundlePageDiagnosticLoggingClient.h"
#include "InjectedBundlePageEditorClient.h"
#include "InjectedBundlePageFullScreenClient.h"
#include "InjectedBundlePageLoaderClient.h"
#include "InjectedBundlePagePolicyClient.h"
#include "InjectedBundlePageResourceLoadClient.h"
#include "LayerTreeHost.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "Plugin.h"
#include "SandboxExtension.h"
#include "ShareableBitmap.h"
#include <WebCore/DictationAlternative.h>
#include <WebCore/DragData.h>
#include <WebCore/Editor.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSizeHash.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PageVisibilityState.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/TextChecking.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/UserActivity.h>
#include <WebCore/ViewState.h>
#include <WebCore/ViewportConfiguration.h>
#include <WebCore/WebCoreKeyboardUIMode.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

#if HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(EFL))
#include "WebPageAccessibilityObject.h"
#include <wtf/gobject/GRefPtr.h>
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#include "WebPrintOperationGtk.h"
#endif

#if PLATFORM(IOS)
#include "GestureTypes.h"
#import "WebPageMessages.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#if PLATFORM(IOS)
#include <WebKitAdditions/PlatformTouchEventIOS.h>
#else
#include <WebCore/PlatformTouchEvent.h>
#endif
#endif

#if ENABLE(CONTEXT_MENUS)
#include "InjectedBundlePageContextMenuClient.h"
#endif

#if PLATFORM(COCOA)
#include "ViewGestureGeometryCollector.h"
#include <wtf/RetainPtr.h>
OBJC_CLASS CALayer;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS WKAccessibilityWebPageObject;

#define ENABLE_PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC 1
#endif

namespace API {
class Array;
}

namespace IPC {
class ArgumentDecoder;
class Connection;
}

namespace WebCore {
class DocumentLoader;
class GraphicsContext;
class Frame;
class FrameView;
class HTMLPlugInElement;
class HTMLPlugInImageElement;
class IntPoint;
class KeyboardEvent;
class Page;
class PrintContext;
class Range;
class ResourceResponse;
class ResourceRequest;
class SharedBuffer;
class SubstituteData;
class TextCheckingRequest;
class URL;
class VisibleSelection;
struct Highlight;
struct KeypressCommand;
struct TextCheckingResult;
}

namespace WebKit {

class DrawingArea;
class InjectedBundleBackForwardList;
class NotificationPermissionRequestManager;
class PageBanner;
class PluginView;
class VisibleContentRectUpdateInfo;
class WebColorChooser;
class WebContextMenu;
class WebContextMenuItemData;
class WebEvent;
class WebFrame;
class WebFullScreenManager;
class WebImage;
class WebInspector;
class WebInspectorClient;
class WebKeyboardEvent;
class WebMouseEvent;
class WebNotificationClient;
class WebOpenPanelResultListener;
class WebPageGroupProxy;
class WebPageOverlay;
class WebPopupMenu;
class WebUndoStep;
class WebUserContentController;
class WebVideoFullscreenManager;
class WebWheelEvent;
struct AssistedNodeInformation;
struct AttributedString;
struct BackForwardListItemState;
struct EditingRange;
struct EditorState;
struct InteractionInformationAtPosition;
struct PrintInfo;
struct WebPageCreationParameters;
struct WebPreferencesStore;

#if PLATFORM(COCOA)
class RemoteLayerTreeTransaction;
#endif

#if ENABLE(TOUCH_EVENTS)
class WebTouchEvent;
#endif

class WebPage : public API::ObjectImpl<API::Object::Type::BundlePage>, public IPC::MessageReceiver, public IPC::MessageSender {
public:
    static PassRefPtr<WebPage> create(uint64_t pageID, const WebPageCreationParameters&);
    virtual ~WebPage();

    void reinitializeWebPage(const WebPageCreationParameters&);

    void close();

    static WebPage* fromCorePage(WebCore::Page*);

    WebCore::Page* corePage() const { return m_page.get(); }
    uint64_t pageID() const { return m_pageID; }
    WebCore::SessionID sessionID() const { return m_page->sessionID(); }
    bool usesEphemeralSession() const { return m_page->usesEphemeralSession(); }

    void setSessionID(WebCore::SessionID);

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }
    
    InjectedBundleBackForwardList* backForwardList();
    DrawingArea* drawingArea() const { return m_drawingArea.get(); }
#if ENABLE(ASYNC_SCROLLING)
    WebCore::ScrollingCoordinator* scrollingCoordinator() const;
#endif

    WebPageGroupProxy* pageGroup() const { return m_pageGroup.get(); }

    void scrollMainFrameIfNotAtMaxScrollPosition(const WebCore::IntSize& scrollOffset);

    bool scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity);

    void centerSelectionInVisibleArea();

#if PLATFORM(COCOA)
    void willCommitLayerTree(RemoteLayerTreeTransaction&);
    void didFlushLayerTreeAtTime(std::chrono::milliseconds);
#endif

#if ENABLE(INSPECTOR)
    WebInspector* inspector();
#endif
    
#if PLATFORM(IOS)
    WebVideoFullscreenManager* videoFullscreenManager();
#endif

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManager* fullScreenManager();
#endif

    // -- Called by the DrawingArea.
    // FIXME: We could genericize these into a DrawingArea client interface. Would that be beneficial?
    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect&);
    void layoutIfNeeded();

    // -- Called from WebCore clients.
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent*);

    void didStartPageTransition();
    void didCompletePageTransition();
    void didCommitLoad(WebFrame*);
    void didFinishDocumentLoad(WebFrame*);
    void didFinishLoad(WebFrame*);
    void show();
    String userAgent(const WebCore::URL&) const;
    String userAgent(WebFrame*, const WebCore::URL&) const;
    String platformUserAgent(const WebCore::URL&) const;
    WebCore::IntRect windowResizerRect() const;
    WebCore::KeyboardUIMode keyboardUIMode();

    WebUndoStep* webUndoStep(uint64_t);
    void addWebUndoStep(uint64_t, WebUndoStep*);
    void removeWebEditCommand(uint64_t);
    bool isInRedo() const { return m_isInRedo; }

    void setActivePopupMenu(WebPopupMenu*);

#if ENABLE(INPUT_TYPE_COLOR)
    WebColorChooser* activeColorChooser() const { return m_activeColorChooser; }
    void setActiveColorChooser(WebColorChooser*);
    void didChooseColor(const WebCore::Color&);
    void didEndColorPicker();
#endif

    WebOpenPanelResultListener* activeOpenPanelResultListener() const { return m_activeOpenPanelResultListener.get(); }
    void setActiveOpenPanelResultListener(PassRefPtr<WebOpenPanelResultListener>);

    void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

    // -- InjectedBundle methods
#if ENABLE(CONTEXT_MENUS)
    void initializeInjectedBundleContextMenuClient(WKBundlePageContextMenuClientBase*);
#endif
    void initializeInjectedBundleEditorClient(WKBundlePageEditorClientBase*);
    void setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient>);
    void initializeInjectedBundleLoaderClient(WKBundlePageLoaderClientBase*);
    void initializeInjectedBundlePolicyClient(WKBundlePagePolicyClientBase*);
    void initializeInjectedBundleResourceLoadClient(WKBundlePageResourceLoadClientBase*);
    void setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient>);
#if ENABLE(FULLSCREEN_API)
    void initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClientBase*);
#endif
    void initializeInjectedBundleDiagnosticLoggingClient(WKBundlePageDiagnosticLoggingClientBase*);

#if ENABLE(CONTEXT_MENUS)
    InjectedBundlePageContextMenuClient& injectedBundleContextMenuClient() { return m_contextMenuClient; }
#endif
    InjectedBundlePageEditorClient& injectedBundleEditorClient() { return m_editorClient; }
    API::InjectedBundle::FormClient& injectedBundleFormClient() { return *m_formClient.get(); }
    InjectedBundlePageLoaderClient& injectedBundleLoaderClient() { return m_loaderClient; }
    InjectedBundlePagePolicyClient& injectedBundlePolicyClient() { return m_policyClient; }
    InjectedBundlePageResourceLoadClient& injectedBundleResourceLoadClient() { return m_resourceLoadClient; }
    API::InjectedBundle::PageUIClient& injectedBundleUIClient() { return *m_uiClient.get(); }
    InjectedBundlePageDiagnosticLoggingClient& injectedBundleDiagnosticLoggingClient() { return m_logDiagnosticMessageClient; }
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient& injectedBundleFullScreenClient() { return m_fullScreenClient; }
#endif

    bool findStringFromInjectedBundle(const String&, FindOptions);

    WebFrame* mainWebFrame() const { return m_mainFrame.get(); }

    WebCore::MainFrame* mainFrame() const; // May return 0.
    WebCore::FrameView* mainFrameView() const; // May return 0.

    PassRefPtr<WebCore::Range> currentSelectionAsRange();

#if ENABLE(NETSCAPE_PLUGIN_API)
    PassRefPtr<Plugin> createPlugin(WebFrame*, WebCore::HTMLPlugInElement*, const Plugin::Parameters&, String& newMIMEType);
#endif

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(WebFrame*, const String&);
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(WebFrame*, const String&);
#endif // ENABLE(WEBGL)
    
    EditorState editorState() const;

    String renderTreeExternalRepresentation() const;
    String renderTreeExternalRepresentationForPrinting() const;
    uint64_t renderTreeSize() const;

    void setTracksRepaints(bool);
    bool isTrackingRepaints() const;
    void resetTrackedRepaints();
    PassRefPtr<API::Array> trackedRepaintRects();

    void executeEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    void clearMainFrameName();
    void sendClose();

    void sendSetWindowFrame(const WebCore::FloatRect&);

    double textZoomFactor() const;
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);
    void windowScreenDidChange(uint64_t);

    void scalePage(double scale, const WebCore::IntPoint& origin);
    void scalePageInViewCoordinates(double scale, WebCore::IntPoint centerInViewCoordinates);
    double pageScaleFactor() const;

    void setUseFixedLayout(bool);
    bool useFixedLayout() const { return m_useFixedLayout; }
    void setFixedLayoutSize(const WebCore::IntSize&);

    void listenForLayoutMilestones(uint32_t /* LayoutMilestones */);

    void setSuppressScrollbarAnimations(bool);
    
    void setEnableVerticalRubberBanding(bool);
    void setEnableHorizontalRubberBanding(bool);
    
    void setBackgroundExtendsBeyondPage(bool);

    void setPaginationMode(uint32_t /* WebCore::Pagination::Mode */);
    void setPaginationBehavesLikeColumns(bool);
    void setPageLength(double);
    void setGapBetweenPages(double);

    void postInjectedBundleMessage(const String& messageName, IPC::MessageDecoder&);

    bool drawsBackground() const { return m_drawsBackground; }
    bool drawsTransparentBackground() const { return m_drawsTransparentBackground; }

    void setUnderlayColor(const WebCore::Color& color) { m_underlayColor = color; }
    WebCore::Color underlayColor() const { return m_underlayColor; }

    void stopLoading();
    void stopLoadingFrame(uint64_t frameID);
    bool defersLoading() const;
    void setDefersLoading(bool deferLoading);

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();

    void addPluginView(PluginView*);
    void removePluginView(PluginView*);

    bool isVisible() const { return m_viewState & WebCore::ViewState::IsVisible; }
    bool isVisibleOrOccluded() const { return m_viewState & WebCore::ViewState::IsVisibleOrOccluded; }

    LayerHostingMode layerHostingMode() const { return m_layerHostingMode; }
    void setLayerHostingMode(unsigned);

#if PLATFORM(COCOA)
    void updatePluginsActiveAndFocusedState();
    const WebCore::FloatRect& windowFrameInScreenCoordinates() const { return m_windowFrameInScreenCoordinates; }
    const WebCore::FloatRect& windowFrameInUnflippedScreenCoordinates() const { return m_windowFrameInUnflippedScreenCoordinates; }
    const WebCore::FloatRect& viewFrameInWindowCoordinates() const { return m_viewFrameInWindowCoordinates; }

    bool hasCachedWindowFrame() const { return m_hasCachedWindowFrame; }

#if !PLATFORM(IOS)
    void setTopOverhangImage(PassRefPtr<WebImage>);
    void setBottomOverhangImage(PassRefPtr<WebImage>);
#endif // !PLATFORM(IOS)

    void updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor);
#endif // PLATFORM(COCOA)

    bool windowIsFocused() const;
    bool windowAndWebPageAreFocused() const;

#if !PLATFORM(IOS)
    void setHeaderPageBanner(PassRefPtr<PageBanner>);
    PageBanner* headerPageBanner();
    void setFooterPageBanner(PassRefPtr<PageBanner>);
    PageBanner* footerPageBanner();

    void hidePageBanners();
    void showPageBanners();
    
#endif // !PLATFORM(IOS)

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&);
    
#if PLATFORM(IOS)
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&);
#endif
    
    PassRefPtr<WebImage> scaledSnapshotWithOptions(const WebCore::IntRect&, double additionalScaleFactor, SnapshotOptions);
    PassRefPtr<WebImage> snapshotAtSize(const WebCore::IntRect&, const WebCore::IntSize& bitmapSize, SnapshotOptions);
    PassRefPtr<WebImage> snapshotNode(WebCore::Node&, SnapshotOptions, unsigned maximumPixelCount = std::numeric_limits<unsigned>::max());

    static const WebEvent* currentEvent();

    FindController& findController() { return m_findController; }

#if ENABLE(GEOLOCATION)
    GeolocationPermissionRequestManager& geolocationPermissionRequestManager() { return m_geolocationPermissionRequestManager; }
#endif

#if PLATFORM(IOS)
    WebCore::FloatSize screenSize() const;
    WebCore::FloatSize availableScreenSize() const;
    int32_t deviceOrientation() const { return m_deviceOrientation; }
    void viewportPropertiesDidChange(const WebCore::ViewportArguments&);
    void didReceiveMobileDocType(bool);
    void savePageState(WebCore::HistoryItem&);
    void restorePageState(const WebCore::HistoryItem&);

    void setUseTestingViewportConfiguration(bool useTestingViewport) { m_useTestingViewportConfiguration = useTestingViewport; }
    bool isUsingTestingViewportConfiguration() const { return m_useTestingViewportConfiguration; }

    double minimumPageScaleFactor() const;
    double maximumPageScaleFactor() const;
    bool allowsUserScaling() const;
    bool hasStablePageScaleFactor() const { return m_hasStablePageScaleFactor; }

    void handleTap(const WebCore::IntPoint&);
    void potentialTapAtPosition(uint64_t requestID, const WebCore::FloatPoint&);
    void commitPotentialTap();
    void commitPotentialTapFailed();
    void cancelPotentialTap();
    void tapHighlightAtPosition(uint64_t requestID, const WebCore::FloatPoint&);

    void inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint&);
    void inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint&);

    void blurAssistedNode();
    void selectWithGesture(const WebCore::IntPoint&, uint32_t granularity, uint32_t gestureType, uint32_t gestureState, uint64_t callbackID);
    void updateSelectionWithTouches(const WebCore::IntPoint& point, uint32_t touches, bool baseIsStart, uint64_t callbackID);
    void updateBlockSelectionWithTouch(const WebCore::IntPoint&, uint32_t touch, uint32_t handlePosition);
    void selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, uint32_t gestureType, uint32_t gestureState, uint64_t callbackID);
    void extendSelection(uint32_t granularity);
    void selectWordBackward();
    void moveSelectionByOffset(int32_t offset, uint64_t callbackID);
    void elementDidFocus(WebCore::Node*);
    void elementDidBlur(WebCore::Node*);
    void requestDictationContext(uint64_t callbackID);
    void replaceDictatedText(const String& oldText, const String& newText);
    void replaceSelectedText(const String& oldText, const String& newText);
    void requestAutocorrectionData(const String& textForAutocorrection, uint64_t callbackID);
    void applyAutocorrection(const String& correction, const String& originalText, uint64_t callbackID);
    void syncApplyAutocorrection(const String& correction, const String& originalText, bool& correctionApplied);
    void requestAutocorrectionContext(uint64_t callbackID);
    void getAutocorrectionContext(String& beforeText, String& markedText, String& selectedText, String& afterText, uint64_t& location, uint64_t& length);
    void getPositionInformation(const WebCore::IntPoint&, InteractionInformationAtPosition&);
    void requestPositionInformation(const WebCore::IntPoint&);
    void startInteractionWithElementAtPosition(const WebCore::IntPoint&);
    void stopInteraction();
    void performActionOnElement(uint32_t action);
    void focusNextAssistedNode(bool isForward);
    void setAssistedNodeValue(const String&);
    void setAssistedNodeValueAsNumber(double);
    void setAssistedNodeSelectedIndex(uint32_t index, bool allowMultipleSelection);
    WebCore::IntRect rectForElementAtInteractionLocation();
    void updateSelectionAppearance();

    void dispatchAsynchronousTouchEvents(const Vector<WebTouchEvent, 1>& queue);
    void contentSizeCategoryDidChange(const String&);
    void executeEditCommandWithCallback(const String&, uint64_t callbackID);

    std::chrono::milliseconds eventThrottlingDelay() const;
#if ENABLE(INSPECTOR)
    void showInspectorHighlight(const WebCore::Highlight&);
    void hideInspectorHighlight();

    void showInspectorIndication();
    void hideInspectorIndication();

    void enableInspectorNodeSearch();
    void disableInspectorNodeSearch();
#endif
#endif

    NotificationPermissionRequestManager* notificationPermissionRequestManager();

    void pageDidScroll();
#if USE(TILED_BACKING_STORE)
    void pageDidRequestScroll(const WebCore::IntPoint&);
    void setFixedVisibleContentRect(const WebCore::IntRect&);
    void sendViewportAttributesChanged();
#endif

#if ENABLE(CONTEXT_MENUS)
    WebContextMenu* contextMenu();
    WebContextMenu* contextMenuAtPointInWindow(const WebCore::IntPoint&);
#endif
    
    bool hasLocalDataForURL(const WebCore::URL&);
    String cachedResponseMIMETypeForURL(const WebCore::URL&);
    String cachedSuggestedFilenameForURL(const WebCore::URL&);
    PassRefPtr<WebCore::SharedBuffer> cachedResponseDataForURL(const WebCore::URL&);

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

#if PLATFORM(EFL)
    void setThemePath(const String&);
#endif

#if USE(TILED_BACKING_STORE)
    void commitPageTransitionViewport();
#endif

#if PLATFORM(GTK)
    void setComposition(const String& text, const Vector<WebCore::CompositionUnderline>& underlines, uint64_t selectionStart, uint64_t selectionEnd, uint64_t replacementRangeStart, uint64_t replacementRangeLength);
    void confirmComposition(const String& text, int64_t selectionStart, int64_t selectionLength);
    void cancelComposition();
#endif

    void didChangeSelection();

#if PLATFORM(COCOA)
    void registerUIProcessAccessibilityTokens(const IPC::DataReference& elemenToken, const IPC::DataReference& windowToken);
    WKAccessibilityWebPageObject* accessibilityRemoteObject();
    NSObject *accessibilityObjectForMainFramePlugin();
    const WebCore::FloatPoint& accessibilityPosition() const { return m_accessibilityPosition; }
    
    void sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput);

    void insertTextAsync(const String& text, const EditingRange& replacementRange, bool registerUndoGroup = false);
    void getMarkedRangeAsync(uint64_t callbackID);
    void getSelectedRangeAsync(uint64_t callbackID);
    void characterIndexForPointAsync(const WebCore::IntPoint&, uint64_t callbackID);
    void firstRectForCharacterRangeAsync(const EditingRange&, uint64_t callbackID);
    void setCompositionAsync(const String& text, Vector<WebCore::CompositionUnderline> underlines, const EditingRange& selectionRange, const EditingRange& replacementRange);
    void confirmCompositionAsync();

#if PLATFORM(MAC)
    void insertDictatedTextAsync(const String& text, const EditingRange& replacementRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool registerUndoGroup = false);
    void attributedSubstringForCharacterRangeAsync(const EditingRange&, uint64_t callbackID);
#if !USE(ASYNC_NSTEXTINPUTCLIENT)
    void insertText(const String& text, const EditingRange& replacementRange, bool& handled, EditorState& newState);
    void setComposition(const String& text, Vector<WebCore::CompositionUnderline> underlines, const EditingRange& selectionRange, const EditingRange& replacementRange, EditorState& newState);
    void confirmComposition(EditorState& newState);
    void insertDictatedText(const String& text, const EditingRange& replacementRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool& handled, EditorState& newState);
    void getAttributedSubstringFromRange(const EditingRange&, AttributedString&);
    void getMarkedRange(EditingRange&);
    void getSelectedRange(EditingRange&);
    void characterIndexForPoint(const WebCore::IntPoint point, uint64_t& result);
    void firstRectForCharacterRange(const EditingRange&, WebCore::IntRect& resultRect);
    void executeKeypressCommands(const Vector<WebCore::KeypressCommand>&, bool& handled, EditorState& newState);
    void cancelComposition(EditorState& newState);
#endif
#endif

    void readSelectionFromPasteboard(const WTF::String& pasteboardName, bool& result);
    void getStringSelectionForPasteboard(WTF::String& stringValue);
    void getDataSelectionForPasteboard(const WTF::String pasteboardType, SharedMemory::Handle& handle, uint64_t& size);
    void shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, bool& result);
    void acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent&, bool& result);
    bool performNonEditingBehaviorForSelector(const String&, WebCore::KeyboardEvent*);

#if ENABLE(SERVICE_CONTROLS)
    void replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference&);
#endif

#elif PLATFORM(EFL)
    void confirmComposition(const String& compositionString);
    void setComposition(const WTF::String& compositionString, const WTF::Vector<WebCore::CompositionUnderline>& underlines, uint64_t cursorPosition);
    void cancelComposition();
#elif PLATFORM(GTK)
#if USE(TEXTURE_MAPPER_GL)
    void setAcceleratedCompositingWindowId(int64_t nativeWindowHandle);
#endif
#endif

#if HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(EFL))
    void updateAccessibilityTree();
#endif

    void setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length);
    bool hasCompositionForTesting();
    void confirmCompositionForTesting(const String& compositionString);

    // FIXME: This a dummy message, to avoid breaking the build for platforms that don't require
    // any synchronous messages, and should be removed when <rdar://problem/8775115> is fixed.
    void dummy(bool&);

#if PLATFORM(COCOA)
    bool isSpeaking();
    void speak(const String&);
    void stopSpeaking();

    void performDictionaryLookupForSelection(WebCore::Frame*, const WebCore::VisibleSelection&, WebCore::TextIndicatorPresentationTransition);
#endif

    bool isSmartInsertDeleteEnabled();
    void setSmartInsertDeleteEnabled(bool);

    bool isSelectTrailingWhitespaceEnabled();
    void setSelectTrailingWhitespaceEnabled(bool);

    void replaceSelectionWithText(WebCore::Frame*, const String&);
    void clearSelection();

#if ENABLE(DRAG_SUPPORT)
#if PLATFORM(GTK)
    void performDragControllerAction(uint64_t action, WebCore::DragData);
#else
    void performDragControllerAction(uint64_t action, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t draggingSourceOperationMask, const WTF::String& dragStorageName, uint32_t flags, const SandboxExtension::Handle&, const SandboxExtension::HandleArray&);
#endif
    void dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, uint64_t operation);

    void willPerformLoadDragDestinationAction();
    void mayPerformUploadDragDestinationAction();
#endif // ENABLE(DRAG_SUPPORT)

    void beginPrinting(uint64_t frameID, const PrintInfo&);
    void endPrinting();
    void computePagesForPrinting(uint64_t frameID, const PrintInfo&, uint64_t callbackID);
    void computePagesForPrintingImpl(uint64_t frameID, const PrintInfo&, Vector<WebCore::IntRect>& pageRects, double& totalScaleFactor);
#if PLATFORM(COCOA)
    void drawRectToImage(uint64_t frameID, const PrintInfo&, const WebCore::IntRect&, const WebCore::IntSize&, uint64_t callbackID);
    void drawPagesToPDF(uint64_t frameID, const PrintInfo&, uint32_t first, uint32_t count, uint64_t callbackID);
    void drawPagesToPDFImpl(uint64_t frameID, const PrintInfo&, uint32_t first, uint32_t count, RetainPtr<CFMutableDataRef>& pdfPageData);
#if PLATFORM(IOS)
    void computePagesForPrintingAndStartDrawingToPDF(uint64_t frameID, const PrintInfo&, uint32_t firstPage, PassRefPtr<Messages::WebPage::ComputePagesForPrintingAndStartDrawingToPDF::DelayedReply>);
#endif
#elif PLATFORM(GTK)
    void drawPagesForPrinting(uint64_t frameID, const PrintInfo&, uint64_t callbackID);
    void didFinishPrintOperation(const WebCore::ResourceError&, uint64_t callbackID);
#endif

    void addResourceRequest(unsigned long, const WebCore::ResourceRequest&);
    void removeResourceRequest(unsigned long);

    void setMediaVolume(float);
    void setMayStartMediaWhenInWindow(bool);

    void updateMainFrameScrollOffsetPinning();

    bool mainFrameHasCustomContentProvider() const;
    void addMIMETypeWithCustomContentProvider(const String&);

    void mainFrameDidLayout();

    bool canRunBeforeUnloadConfirmPanel() const { return m_canRunBeforeUnloadConfirmPanel; }
    void setCanRunBeforeUnloadConfirmPanel(bool canRunBeforeUnloadConfirmPanel) { m_canRunBeforeUnloadConfirmPanel = canRunBeforeUnloadConfirmPanel; }

    bool canRunModal() const { return m_canRunModal; }
    void setCanRunModal(bool canRunModal) { m_canRunModal = canRunModal; }

    void runModal();

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const;

    void forceRepaintWithoutCallback();

    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();
#if PLATFORM(COCOA)
    void handleAlternativeTextUIResult(const String&);
#endif

    // For testing purpose.
    void simulateMouseDown(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, double time);
    void simulateMouseUp(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, double time);
    void simulateMouseMotion(WebCore::IntPoint, double time);

#if ENABLE(CONTEXT_MENUS)
    void contextMenuShowing() { m_isShowingContextMenu = true; }
#endif

    void wheelEvent(const WebWheelEvent&);

    void numWheelEventHandlersChanged(unsigned);
    void recomputeShortCircuitHorizontalWheelEventsState();

    void updateVisibilityState(bool isInitialState = false);

#if PLATFORM(IOS)
    void setViewportConfigurationMinimumLayoutSize(const WebCore::FloatSize&);
    void setViewportConfigurationMinimumLayoutSizeForMinimalUI(const WebCore::FloatSize&);
    void setMaximumUnobscuredSize(const WebCore::FloatSize&);
    void setDeviceOrientation(int32_t);
    void dynamicViewportSizeUpdate(const WebCore::FloatSize& minimumLayoutSize, const WebCore::FloatSize& minimumLayoutSizeForMinimalUI, const WebCore::FloatSize& maximumUnobscuredSize, const WebCore::FloatRect& targetExposedContentRect, const WebCore::FloatRect& targetUnobscuredRect, const WebCore::FloatRect& targetUnobscuredRectInScrollViewCoordinates, double scale, int32_t deviceOrientation);
    void synchronizeDynamicViewportUpdate(double& newTargetScale, WebCore::FloatPoint& newScrollPosition, uint64_t& nextValidLayerTreeTransactionID);
    void updateVisibleContentRects(const VisibleContentRectUpdateInfo&, double oldestTimestamp);
    bool scaleWasSetByUIProcess() const { return m_scaleWasSetByUIProcess; }
    void willStartUserTriggeredZooming();
    void applicationWillResignActive();
    void applicationWillEnterForeground();
    void applicationDidBecomeActive();
    void zoomToRect(WebCore::FloatRect, double minimumScale, double maximumScale);
    void dispatchTouchEvent(const WebTouchEvent&, bool& handled);
    void completePendingSyntheticClickForContentChangeObserver();
#endif

#if PLATFORM(GTK) && USE(TEXTURE_MAPPER_GL)
    uint64_t nativeWindowHandle() { return m_nativeWindowHandle; }
#endif

    bool shouldUseCustomContentProviderForResponse(const WebCore::ResourceResponse&);
    bool canPluginHandleResponse(const WebCore::ResourceResponse& response);

    bool asynchronousPluginInitializationEnabled() const { return m_asynchronousPluginInitializationEnabled; }
    void setAsynchronousPluginInitializationEnabled(bool enabled) { m_asynchronousPluginInitializationEnabled = enabled; }
    bool asynchronousPluginInitializationEnabledForAllPlugins() const { return m_asynchronousPluginInitializationEnabledForAllPlugins; }
    void setAsynchronousPluginInitializationEnabledForAllPlugins(bool enabled) { m_asynchronousPluginInitializationEnabledForAllPlugins = enabled; }
    bool artificialPluginInitializationDelayEnabled() const { return m_artificialPluginInitializationDelayEnabled; }
    void setArtificialPluginInitializationDelayEnabled(bool enabled) { m_artificialPluginInitializationDelayEnabled = enabled; }
    void setTabToLinksEnabled(bool enabled) { m_tabToLinks = enabled; }
    bool tabToLinksEnabled() const { return m_tabToLinks; }

    bool scrollingPerformanceLoggingEnabled() const { return m_scrollingPerformanceLoggingEnabled; }
    void setScrollingPerformanceLoggingEnabled(bool);

#if PLATFORM(COCOA)
    bool shouldUsePDFPlugin() const;
    bool pdfPluginEnabled() const { return m_pdfPluginEnabled; }
    void setPDFPluginEnabled(bool enabled) { m_pdfPluginEnabled = enabled; }
#endif

    void savePDFToFileInDownloadsFolder(const String& suggestedFilename, const String& originatingURLString, const uint8_t* data, unsigned long size);
#if PLATFORM(COCOA)
    void savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, const String& originatingURLString, const uint8_t* data, unsigned long size, const String& pdfUUID);
#endif

    bool mainFrameIsScrollable() const { return m_mainFrameIsScrollable; }

    void setMinimumLayoutSize(const WebCore::IntSize&);
    WebCore::IntSize minimumLayoutSize() const { return m_minimumLayoutSize; }

    void setAutoSizingShouldExpandToViewHeight(bool shouldExpand);
    bool autoSizingShouldExpandToViewHeight() { return m_autoSizingShouldExpandToViewHeight; }

    bool canShowMIMEType(const String& MIMEType) const;

    void addTextCheckingRequest(uint64_t requestID, PassRefPtr<WebCore::TextCheckingRequest>);
    void didFinishCheckingText(uint64_t requestID, const Vector<WebCore::TextCheckingResult>&);
    void didCancelCheckingText(uint64_t requestID);

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    void determinePrimarySnapshottedPlugIn();
    void determinePrimarySnapshottedPlugInTimerFired();
    void resetPrimarySnapshottedPlugIn();
    bool matchesPrimaryPlugIn(const String& pageOrigin, const String& pluginOrigin, const String& mimeType) const;
    bool plugInIntersectsSearchRect(WebCore::HTMLPlugInImageElement& pluginImageElement);
    bool plugInIsPrimarySize(WebCore::HTMLPlugInImageElement& pluginImageElement, unsigned &pluginArea);
#endif

    unsigned extendIncrementalRenderingSuppression();
    void stopExtendingIncrementalRenderingSuppression(unsigned token);
    bool shouldExtendIncrementalRenderingSuppression() { return !m_activeRenderingSuppressionTokens.isEmpty(); }

    WebCore::ScrollPinningBehavior scrollPinningBehavior() { return m_scrollPinningBehavior; }
    void setScrollPinningBehavior(uint32_t /* WebCore::ScrollPinningBehavior */ pinning);

    PassRefPtr<WebCore::DocumentLoader> createDocumentLoader(WebCore::Frame&, const WebCore::ResourceRequest&, const WebCore::SubstituteData&);

    void getBytecodeProfile(uint64_t callbackID);
    
    // Some platforms require accessibility-enabled processes to spin the run loop so that the WebProcess doesn't hang.
    // While this is not ideal, it does not have to be applied to every platform at the moment.
    static bool synchronousMessagesShouldSpinRunLoop();

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    void handleTelephoneNumberClick(const String& number, const WebCore::IntPoint&);
    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&);
#endif

    void didChangeScrollOffsetForFrame(WebCore::Frame*);

    void willChangeCurrentHistoryItemForMainFrame();
    bool shouldDispatchFakeMouseMoveEvents() const { return m_shouldDispatchFakeMouseMoveEvents; }

private:
    WebPage(uint64_t pageID, const WebPageCreationParameters&);

    // IPC::MessageSender
    virtual IPC::Connection* messageSenderConnection() override;
    virtual uint64_t messageSenderDestinationID() override;

    void platformInitialize();

    void didReceiveWebPageMessage(IPC::Connection*, IPC::MessageDecoder&);
    void didReceiveSyncWebPageMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

#if PLATFORM(IOS)
    void resetViewportDefaultConfiguration(WebFrame* mainFrame);
    void viewportConfigurationChanged();
    void updateViewportSizeForCSSViewportUnits();

    static void convertSelectionRectsToRootView(WebCore::FrameView*, Vector<WebCore::SelectionRect>&);
    PassRefPtr<WebCore::Range> rangeForWebSelectionAtPosition(const WebCore::IntPoint&, const WebCore::VisiblePosition&, SelectionFlags&);
    PassRefPtr<WebCore::Range> rangeForBlockAtPoint(const WebCore::IntPoint&);
    void computeExpandAndShrinkThresholdsForHandle(const WebCore::IntPoint&, SelectionHandlePosition, float& growThreshold, float& shrinkThreshold);
    PassRefPtr<WebCore::Range> changeBlockSelection(const WebCore::IntPoint&, SelectionHandlePosition, float& growThreshold, float& shrinkThreshold, SelectionFlags&);
    PassRefPtr<WebCore::Range> expandedRangeFromHandle(WebCore::Range*, SelectionHandlePosition);
    PassRefPtr<WebCore::Range> contractedRangeFromHandle(WebCore::Range* currentRange, SelectionHandlePosition, SelectionFlags&);
    void getAssistedNodeInformation(AssistedNodeInformation&);
    void platformInitializeAccessibility();
    void handleSyntheticClick(WebCore::Node* nodeRespondingToClick, const WebCore::FloatPoint& location);
    void completeSyntheticClick(WebCore::Node* nodeRespondingToClick, const WebCore::FloatPoint& location);
    void sendTapHighlightForNodeIfNecessary(uint64_t requestID, WebCore::Node*);
    void resetTextAutosizingBeforeLayoutIfNeeded(const WebCore::FloatSize& oldSize, const WebCore::FloatSize& newSize);
#endif
#if !PLATFORM(COCOA)
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
#endif
    bool performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&);

#if PLATFORM(MAC)
    bool executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>&, WebCore::KeyboardEvent*);
#endif

    String sourceForFrame(WebFrame*);

    void loadDataImpl(uint64_t navigationID, PassRefPtr<WebCore::SharedBuffer>, const String& MIMEType, const String& encodingName, const WebCore::URL& baseURL, const WebCore::URL& failingURL, IPC::MessageDecoder&);
    void loadString(uint64_t navigationID, const String&, const String& MIMEType, const WebCore::URL& baseURL, const WebCore::URL& failingURL, IPC::MessageDecoder&);

    bool platformHasLocalDataForURL(const WebCore::URL&);

    // Actions
    void tryClose();
    void loadRequest(uint64_t navigationID, const WebCore::ResourceRequest&, const SandboxExtension::Handle&, IPC::MessageDecoder&);
    void loadData(const IPC::DataReference&, const String& MIMEType, const String& encodingName, const String& baseURL, IPC::MessageDecoder&);
    void loadHTMLString(uint64_t navigationID, const String& htmlString, const String& baseURL, IPC::MessageDecoder&);
    void loadAlternateHTMLString(const String& htmlString, const String& baseURL, const String& unreachableURL, IPC::MessageDecoder&);
    void loadPlainTextString(const String&, IPC::MessageDecoder&);
    void loadWebArchiveData(const IPC::DataReference&, IPC::MessageDecoder&);
    void reload(uint64_t navigationID, bool reloadFromOrigin, const SandboxExtension::Handle&);
    void goForward(uint64_t navigationID, uint64_t);
    void goBack(uint64_t navigationID, uint64_t);
    void goToBackForwardItem(uint64_t navigationID, uint64_t);
    void tryRestoreScrollPosition();
    void setActive(bool);
    void setFocused(bool);
    void setViewIsVisible(bool);
    void setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent&);
    void setWindowResizerSize(const WebCore::IntSize&);
    void updateIsInWindow(bool isInitialState = false);
    void setViewState(WebCore::ViewState::Flags, bool wantsDidUpdateViewState, const Vector<uint64_t>& callbackIDs);
    void validateCommand(const String&, uint64_t);
    void executeEditCommand(const String&);

    void mouseEvent(const WebMouseEvent&);
    void mouseEventSyncForTesting(const WebMouseEvent&, bool&);
    void wheelEventSyncForTesting(const WebWheelEvent&, bool&);
    void keyEvent(const WebKeyboardEvent&);
    void keyEventSyncForTesting(const WebKeyboardEvent&, bool&);
#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEventSync(const WebTouchEvent&, bool& handled);
#elif ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
    void touchEventSyncForTesting(const WebTouchEvent&, bool& handled);
#endif
#if ENABLE(CONTEXT_MENUS)
    void contextMenuHidden() { m_isShowingContextMenu = false; }
#endif

    static bool scroll(WebCore::Page*, WebCore::ScrollDirection, WebCore::ScrollGranularity);
    static bool logicalScroll(WebCore::Page*, WebCore::ScrollLogicalDirection, WebCore::ScrollGranularity);

    void loadURLInFrame(const String&, uint64_t frameID);

    void restoreSession(const Vector<BackForwardListItemState>&);
    void didRemoveBackForwardItem(uint64_t);

#if ENABLE(REMOTE_INSPECTOR)
    void setAllowsRemoteInspection(bool);
#endif

    void setDrawsBackground(bool);
    void setDrawsTransparentBackground(bool);

    void setTopContentInset(float);

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void getContentsAsString(uint64_t callbackID);
#if ENABLE(MHTML)
    void getContentsAsMHTMLData(uint64_t callbackID, bool useBinaryEncoding);
#endif
    void getMainResourceDataOfFrame(uint64_t frameID, uint64_t callbackID);
    void getResourceDataFromFrame(uint64_t frameID, const String& resourceURL, uint64_t callbackID);
    void getRenderTreeExternalRepresentation(uint64_t callbackID);
    void getSelectionOrContentsAsString(uint64_t callbackID);
    void getSelectionAsWebArchiveData(uint64_t callbackID);
    void getSourceForFrame(uint64_t frameID, uint64_t callbackID);
    void getWebArchiveOfFrame(uint64_t frameID, uint64_t callbackID);
    void runJavaScriptInMainFrame(const String&, uint64_t callbackID);
    void forceRepaint(uint64_t callbackID);
    void takeSnapshot(WebCore::IntRect snapshotRect, WebCore::IntSize bitmapSize, uint32_t options, uint64_t callbackID);

    void preferencesDidChange(const WebPreferencesStore&);
    void platformPreferencesDidChange(const WebPreferencesStore&);
    void updatePreferences(const WebPreferencesStore&);

    void didReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction, uint64_t navigationID, uint64_t downloadID);
    void setUserAgent(const String&);
    void setCustomTextEncodingName(const String&);
    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

#if PLATFORM(COCOA)
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    void performDictionaryLookupOfCurrentSelection();
    void performDictionaryLookupForRange(WebCore::Frame*, WebCore::Range&, NSDictionary *options, WebCore::TextIndicatorPresentationTransition);
    DictionaryPopupInfo dictionaryPopupInfoForRange(WebCore::Frame* frame, WebCore::Range& range, NSDictionary **options, WebCore::TextIndicatorPresentationTransition presentationTransition);

    void windowAndViewFramesChanged(const WebCore::FloatRect& windowFrameInScreenCoordinates, const WebCore::FloatRect& windowFrameInUnflippedScreenCoordinates, const WebCore::FloatRect& viewFrameInWindowCoordinates, const WebCore::FloatPoint& accessibilityViewCoordinates);

    RetainPtr<PDFDocument> pdfDocumentForPrintingFrame(WebCore::Frame*);
    void computePagesForPrintingPDFDocument(uint64_t frameID, const PrintInfo&, Vector<WebCore::IntRect>& resultPageRects);
    void drawPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, const WebCore::IntRect&);
    void drawPagesToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, uint32_t first, uint32_t count);
#endif

    void setMainFrameIsScrollable(bool);

    void unapplyEditCommand(uint64_t commandID);
    void reapplyEditCommand(uint64_t commandID);
    void didRemoveEditCommand(uint64_t commandID);

    void findString(const String&, uint32_t findOptions, uint32_t maxMatchCount);
    void findStringMatches(const String&, uint32_t findOptions, uint32_t maxMatchCount);
    void getImageForFindMatch(uint32_t matchIndex);
    void selectFindMatch(uint32_t matchIndex);
    void hideFindUI();
    void countStringMatches(const String&, uint32_t findOptions, uint32_t maxMatchCount);

#if USE(COORDINATED_GRAPHICS)
    void findZoomableAreaForPoint(const WebCore::IntPoint&, const WebCore::IntSize& area);
#endif

    void didChangeSelectedIndexForActivePopupMenu(int32_t newIndex);
    void setTextForActivePopupMenu(int32_t index);

#if PLATFORM(GTK)
    void failedToShowPopupMenu();
#endif

#if PLATFORM(IOS)
    void didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>&, const String& displayString, const IPC::DataReference& iconData);
#endif
    void didChooseFilesForOpenPanel(const Vector<String>&);
    void didCancelForOpenPanel();
#if ENABLE(SANDBOX_EXTENSIONS)
    void extendSandboxForFileFromOpenPanel(const SandboxExtension::Handle&);
#endif

    void didReceiveGeolocationPermissionDecision(uint64_t geolocationID, bool allowed);

    void didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed);

    void advanceToNextMisspelling(bool startBeforeSelection);
    void changeSpellingToWord(const String& word);
#if USE(APPKIT)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();
#endif

#if ENABLE(CONTEXT_MENUS)
    void didSelectItemFromActiveContextMenu(const WebContextMenuItemData&);
#endif

    void changeSelectedIndex(int32_t index);
    void setCanStartMediaTimerFired();

    bool canHandleUserEvents() const;

    static bool platformCanHandleRequest(const WebCore::ResourceRequest&);

    static PluginView* focusedPluginViewForFrame(WebCore::Frame&);
    static PluginView* pluginViewForFrame(WebCore::Frame*);

    static PassRefPtr<WebCore::Range> rangeFromEditingRange(WebCore::Frame&, const EditingRange&);

    void reportUsedFeatures();

#if PLATFORM(MAC)
    void performActionMenuHitTestAtLocation(WebCore::FloatPoint, bool forImmediateAction);
    PassRefPtr<WebCore::Range> lookupTextAtLocation(WebCore::FloatPoint, NSDictionary **options);
    void selectLastActionMenuRange();
    void focusAndSelectLastActionMenuHitTestResult();

    void dataDetectorsDidPresentUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidChangeUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidHideUI(WebCore::PageOverlay::PageOverlayID);
#endif

    void setShouldDispatchFakeMouseMoveEvents(bool dispatch) { m_shouldDispatchFakeMouseMoveEvents = dispatch; }

    uint64_t m_pageID;

    std::unique_ptr<WebCore::Page> m_page;
    RefPtr<WebFrame> m_mainFrame;
    RefPtr<InjectedBundleBackForwardList> m_backForwardList;

    RefPtr<WebPageGroupProxy> m_pageGroup;

    String m_userAgent;

    WebCore::IntSize m_viewSize;
    std::unique_ptr<DrawingArea> m_drawingArea;

    HashSet<PluginView*> m_pluginViews;
    bool m_hasSeenPlugin;

    HashMap<uint64_t, RefPtr<WebCore::TextCheckingRequest>> m_pendingTextCheckingRequestMap;

    bool m_useFixedLayout;

    bool m_drawsBackground;
    bool m_drawsTransparentBackground;

    WebCore::Color m_underlayColor;

    bool m_isInRedo;
    bool m_isClosed;

    bool m_tabToLinks;
    
    bool m_asynchronousPluginInitializationEnabled;
    bool m_asynchronousPluginInitializationEnabledForAllPlugins;
    bool m_artificialPluginInitializationDelayEnabled;

    bool m_scrollingPerformanceLoggingEnabled;

    bool m_mainFrameIsScrollable;

#if ENABLE(PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
    bool m_readyToFindPrimarySnapshottedPlugin;
    bool m_didFindPrimarySnapshottedPlugin;
    unsigned m_numberOfPrimarySnapshotDetectionAttempts;
    String m_primaryPlugInPageOrigin;
    String m_primaryPlugInOrigin;
    String m_primaryPlugInMimeType;
    RunLoop::Timer<WebPage> m_determinePrimarySnapshottedPlugInTimer;
#endif

    // The layer hosting mode.
    LayerHostingMode m_layerHostingMode;

#if PLATFORM(COCOA)
    bool m_pdfPluginEnabled;

    bool m_hasCachedWindowFrame;

    // The frame of the containing window in screen coordinates.
    WebCore::FloatRect m_windowFrameInScreenCoordinates;

    // The frame of the containing window in unflipped screen coordinates.
    WebCore::FloatRect m_windowFrameInUnflippedScreenCoordinates;

    // The frame of the view in window coordinates.
    WebCore::FloatRect m_viewFrameInWindowCoordinates;

    // The accessibility position of the view.
    WebCore::FloatPoint m_accessibilityPosition;
    
    RetainPtr<WKAccessibilityWebPageObject> m_mockAccessibilityElement;

    ViewGestureGeometryCollector m_viewGestureGeometryCollector;

#elif HAVE(ACCESSIBILITY) && (PLATFORM(GTK) || PLATFORM(EFL))
    GRefPtr<WebPageAccessibilityObject> m_accessibilityObject;
#endif

#if PLATFORM(GTK) && USE(TEXTURE_MAPPER_GL)
    // Our view's window in the UI process.
    uint64_t m_nativeWindowHandle;
#endif

#if !PLATFORM(IOS)
    RefPtr<PageBanner> m_headerBanner;
    RefPtr<PageBanner> m_footerBanner;
#endif // !PLATFORM(IOS)

    RunLoop::Timer<WebPage> m_setCanStartMediaTimer;
    bool m_mayStartMediaWhenInWindow;

    HashMap<uint64_t, RefPtr<WebUndoStep>> m_undoStepMap;

    WebCore::IntSize m_windowResizerSize;

#if ENABLE(CONTEXT_MENUS)
    InjectedBundlePageContextMenuClient m_contextMenuClient;
#endif
    InjectedBundlePageEditorClient m_editorClient;
    std::unique_ptr<API::InjectedBundle::FormClient> m_formClient;
    InjectedBundlePageLoaderClient m_loaderClient;
    InjectedBundlePagePolicyClient m_policyClient;
    InjectedBundlePageResourceLoadClient m_resourceLoadClient;
    std::unique_ptr<API::InjectedBundle::PageUIClient> m_uiClient;
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient m_fullScreenClient;
#endif
    InjectedBundlePageDiagnosticLoggingClient m_logDiagnosticMessageClient;

    FindController m_findController;

#if ENABLE(INSPECTOR)
    RefPtr<WebInspector> m_inspector;
#endif
#if PLATFORM(IOS)
    RefPtr<WebVideoFullscreenManager> m_videoFullscreenManager;
#endif
#if ENABLE(FULLSCREEN_API)
    RefPtr<WebFullScreenManager> m_fullScreenManager;
#endif
    RefPtr<WebPopupMenu> m_activePopupMenu;
#if ENABLE(CONTEXT_MENUS)
    RefPtr<WebContextMenu> m_contextMenu;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    WebColorChooser* m_activeColorChooser;
#endif
    RefPtr<WebOpenPanelResultListener> m_activeOpenPanelResultListener;
    RefPtr<NotificationPermissionRequestManager> m_notificationPermissionRequestManager;

    RefPtr<WebUserContentController> m_userContentController;

#if ENABLE(GEOLOCATION)
    GeolocationPermissionRequestManager m_geolocationPermissionRequestManager;
#endif

    std::unique_ptr<WebCore::PrintContext> m_printContext;
#if PLATFORM(GTK)
    RefPtr<WebPrintOperationGtk> m_printOperation;
#endif

    SandboxExtensionTracker m_sandboxExtensionTracker;

    RefPtr<SandboxExtension> m_pendingDropSandboxExtension;
    Vector<RefPtr<SandboxExtension>> m_pendingDropExtensionsForFileUpload;

    bool m_canRunBeforeUnloadConfirmPanel;

    bool m_canRunModal;
    bool m_isRunningModal;

    bool m_cachedMainFrameIsPinnedToLeftSide;
    bool m_cachedMainFrameIsPinnedToRightSide;
    bool m_cachedMainFrameIsPinnedToTopSide;
    bool m_cachedMainFrameIsPinnedToBottomSide;
    bool m_canShortCircuitHorizontalWheelEvents;
    unsigned m_numWheelEventHandlers;

    unsigned m_cachedPageCount;

    HashSet<unsigned long> m_networkResourceRequestIdentifiers;

    WebCore::IntSize m_minimumLayoutSize;
    bool m_autoSizingShouldExpandToViewHeight;

#if ENABLE(CONTEXT_MENUS)
    bool m_isShowingContextMenu;
#endif
    
#if PLATFORM(IOS)
    RefPtr<WebCore::Node> m_assistedNode;
    RefPtr<WebCore::Range> m_currentWordRange;
    RefPtr<WebCore::Node> m_interactionNode;
    WebCore::IntPoint m_lastInteractionLocation;

    RefPtr<WebCore::Node> m_potentialTapNode;
    WebCore::FloatPoint m_potentialTapLocation;

    WebCore::ViewportConfiguration m_viewportConfiguration;
    uint64_t m_firstLayerTreeTransactionIDAfterDidCommitLoad;
    bool m_hasReceivedVisibleContentRectsAfterDidCommitLoad;
    bool m_scaleWasSetByUIProcess;
    bool m_userHasChangedPageScaleFactor;
    bool m_hasStablePageScaleFactor;
    bool m_userIsInteracting;
    bool m_hasPendingBlurNotification;
    bool m_useTestingViewportConfiguration;
    bool m_isInStableState;
    std::chrono::milliseconds m_oldestNonStableUpdateVisibleContentRectsTimestamp;
    std::chrono::milliseconds m_estimatedLatency;
    WebCore::FloatSize m_screenSize;
    WebCore::FloatSize m_availableScreenSize;
    RefPtr<WebCore::Range> m_currentBlockSelection;
    WebCore::IntSize m_blockSelectionDesiredSize;
    WebCore::FloatSize m_maximumUnobscuredSize;
    int32_t m_deviceOrientation;
    bool m_inDynamicSizeUpdate;
    HashMap<std::pair<WebCore::IntSize, double>, WebCore::IntPoint> m_dynamicSizeUpdateHistory;
    RefPtr<WebCore::Node> m_pendingSyntheticClickNode;
    WebCore::FloatPoint m_pendingSyntheticClickLocation;
#endif

    WebInspectorClient* m_inspectorClient;

    HashSet<String, CaseFoldingHash> m_mimeTypesWithCustomContentProviders;
    WebCore::Color m_backgroundColor;

    HashSet<unsigned> m_activeRenderingSuppressionTokens;
    unsigned m_maximumRenderingSuppressionToken;
    
    WebCore::ScrollPinningBehavior m_scrollPinningBehavior;

    bool m_useAsyncScrolling;

    WebCore::ViewState::Flags m_viewState;

    UserActivity m_processSuppressionDisabledByWebPreference;

    uint64_t m_pendingNavigationID;

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy m_systemWebGLPolicy;
#endif

#if PLATFORM(MAC)
    RefPtr<WebCore::Range> m_lastActionMenuRangeForSelection;
    WebCore::HitTestResult m_lastActionMenuHitTestResult;
    RefPtr<WebPageOverlay> m_lastActionMenuHitPageOverlay;
#endif

    bool m_shouldDispatchFakeMouseMoveEvents;
};

} // namespace WebKit

#endif // WebPage_h
