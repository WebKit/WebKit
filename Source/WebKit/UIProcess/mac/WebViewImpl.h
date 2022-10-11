/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "DrawingAreaInfo.h"
#include "ImageAnalysisUtilities.h"
#include "PDFPluginIdentifier.h"
#include "ShareableBitmap.h"
#include "WKLayoutMode.h"
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/TextIndicatorWindow.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebKit/WKDragDestinationAction.h>
#include <WebKit/_WKOverlayScrollbarStyle.h>
#include <pal/spi/cocoa/AVKitSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RetainPtr.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

using _WKRectEdge = NSUInteger;

OBJC_CLASS NSAccessibilityRemoteUIElement;
OBJC_CLASS NSImmediateActionGestureRecognizer;
OBJC_CLASS NSMenu;
OBJC_CLASS NSPopover;
OBJC_CLASS NSTextInputContext;
OBJC_CLASS NSView;
OBJC_CLASS QLPreviewPanel;
OBJC_CLASS WKAccessibilitySettingsObserver;
OBJC_CLASS WKBrowsingContextController;
OBJC_CLASS WKDOMPasteMenuDelegate;
OBJC_CLASS WKEditorUndoTarget;
OBJC_CLASS WKFullScreenWindowController;
OBJC_CLASS WKImageAnalysisOverlayViewDelegate;
OBJC_CLASS WKImmediateActionController;
OBJC_CLASS WKMouseTrackingObserver;
OBJC_CLASS WKRevealItemPresenter;
OBJC_CLASS WKSafeBrowsingWarning;
OBJC_CLASS WKShareSheet;
OBJC_CLASS WKViewLayoutStrategy;
OBJC_CLASS WKWebView;
OBJC_CLASS WKWindowVisibilityObserver;
OBJC_CLASS _WKRemoteObjectRegistry;
OBJC_CLASS _WKThumbnailView;

#if HAVE(TOUCH_BAR)
OBJC_CLASS NSCandidateListTouchBarItem;
OBJC_CLASS NSCustomTouchBarItem;
OBJC_CLASS NSTouchBar;
OBJC_CLASS NSTouchBarItem;
OBJC_CLASS NSPopoverTouchBarItem;
OBJC_CLASS WKTextTouchBarItemController;
OBJC_CLASS WebPlaybackControlsManager;
#endif // HAVE(TOUCH_BAR)

#if ENABLE(UI_PROCESS_PDF_HUD)
OBJC_CLASS WKPDFHUDView;
#endif

OBJC_CLASS VKCImageAnalysis;
OBJC_CLASS VKCImageAnalysisOverlayView;

namespace API {
class HitTestResult;
class Object;
class PageConfiguration;
}

namespace WebCore {
class DestinationColorSpace;
class IntPoint;
struct DataDetectorElementInfo;
struct ShareDataWithParsedURL;
struct TextRecognitionResult;

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
struct TranslationContextMenuInfo;
#endif
}

@protocol WebViewImplDelegate

- (NSTextInputContext *)_web_superInputContext;
- (void)_web_superQuickLookWithEvent:(NSEvent *)event;
- (void)_web_superRemoveTrackingRect:(NSTrackingRectTag)tag;
- (void)_web_superSwipeWithEvent:(NSEvent *)event;
- (void)_web_superMagnifyWithEvent:(NSEvent *)event;
- (void)_web_superSmartMagnifyWithEvent:(NSEvent *)event;
- (id)_web_superAccessibilityAttributeValue:(NSString *)attribute;
- (void)_web_superDoCommandBySelector:(SEL)selector;
- (BOOL)_web_superPerformKeyEquivalent:(NSEvent *)event;
- (void)_web_superKeyDown:(NSEvent *)event;
- (NSView *)_web_superHitTest:(NSPoint)point;

- (id)_web_immediateActionAnimationControllerForHitTestResultInternal:(API::HitTestResult*)hitTestResult withType:(uint32_t)type userData:(API::Object*)userData;
- (void)_web_prepareForImmediateActionAnimation;
- (void)_web_cancelImmediateActionAnimation;
- (void)_web_completeImmediateActionAnimation;

- (void)_web_dismissContentRelativeChildWindows;
- (void)_web_dismissContentRelativeChildWindowsWithAnimation:(BOOL)animate;
- (void)_web_editorStateDidChange;

- (void)_web_gestureEventWasNotHandledByWebCore:(NSEvent *)event;

- (void)_web_didChangeContentSize:(NSSize)newSize;

#if ENABLE(DRAG_SUPPORT)
- (WKDragDestinationAction)_web_dragDestinationActionForDraggingInfo:(id <NSDraggingInfo>)draggingInfo;
- (void)_web_didPerformDragOperation:(BOOL)handled;
#endif

@optional
- (void)_web_didAddMediaControlsManager:(id)controlsManager;
- (void)_web_didRemoveMediaControlsManager;
- (void)_didHandleAcceptedCandidate;
- (void)_didUpdateCandidateListVisibility:(BOOL)visible;

@end

namespace WebCore {
struct DragItem;
struct KeypressCommand;
}

namespace WebKit {

class PageClient;
class PageClientImpl;
class DrawingAreaProxy;
class MediaSessionCoordinatorProxyPrivate;
class SafeBrowsingWarning;
class ViewGestureController;
class ViewSnapshot;
class WebBackForwardListItem;
class WebEditCommandProxy;
class WebFrameProxy;
class WebPageProxy;
class WebProcessPool;
class WebProcessProxy;
struct WebHitTestResultData;

enum class ContinueUnsafeLoad : bool;
enum class UndoOrRedo : bool;

typedef id <NSValidatedUserInterfaceItem> ValidationItem;
typedef Vector<RetainPtr<ValidationItem>> ValidationVector;
typedef HashMap<String, ValidationVector> ValidationMap;

class WebViewImpl : public CanMakeWeakPtr<WebViewImpl> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WebViewImpl);
public:
    WebViewImpl(NSView <WebViewImplDelegate> *, WKWebView *outerWebView, WebProcessPool&, Ref<API::PageConfiguration>&&);

    ~WebViewImpl();

    NSWindow *window();

    WebPageProxy& page() { return m_page.get(); }
    NSView *view() const { return m_view.getAutoreleased(); }

    void processWillSwap();
    void processDidExit();
    void pageClosed();
    void didRelaunchProcess();

    void setDrawsBackground(bool);
    bool drawsBackground() const;
    void setBackgroundColor(NSColor *);
    NSColor *backgroundColor() const;
    bool isOpaque() const;

    void setShouldSuppressFirstResponderChanges(bool);
    bool acceptsFirstMouse(NSEvent *);
    bool acceptsFirstResponder();
    bool becomeFirstResponder();
    bool resignFirstResponder();
    bool isFocused() const;

    void viewWillStartLiveResize();
    void viewDidEndLiveResize();

#if ENABLE(UI_PROCESS_PDF_HUD)
    void createPDFHUD(PDFPluginIdentifier, const WebCore::IntRect&);
    void updatePDFHUDLocation(PDFPluginIdentifier, const WebCore::IntRect&);
    void removePDFHUD(PDFPluginIdentifier);
    void removeAllPDFHUDs();
    NSSet *pdfHUDs();
#endif

    void renewGState();
    void setFrameSize(CGSize);
    void disableFrameSizeUpdates();
    void enableFrameSizeUpdates();
    bool frameSizeUpdatesDisabled() const;
    void setFrameAndScrollBy(CGRect, CGSize);
    void updateWindowAndViewFrames();

    void setFixedLayoutSize(CGSize);
    CGSize fixedLayoutSize() const;

    std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&);
    bool isUsingUISideCompositing() const;
    void setDrawingAreaSize(CGSize);
    void updateLayer();
    static bool wantsUpdateLayer() { return true; }

    void drawRect(CGRect);
    bool canChangeFrameLayout(WebFrameProxy&);
    NSPrintOperation *printOperationWithPrintInfo(NSPrintInfo *, WebFrameProxy&);

    void setAutomaticallyAdjustsContentInsets(bool);
    bool automaticallyAdjustsContentInsets() const { return m_automaticallyAdjustsContentInsets; }
    void updateContentInsetsIfAutomatic();
    void setTopContentInset(CGFloat);
    CGFloat topContentInset() const;

    void prepareContentInRect(CGRect);
    void updateViewExposedRect();
    void setClipsToVisibleRect(bool);
    bool clipsToVisibleRect() const { return m_clipsToVisibleRect; }

    void setMinimumSizeForAutoLayout(CGSize);
    CGSize minimumSizeForAutoLayout() const;
    void setSizeToContentAutoSizeMaximumSize(CGSize);
    CGSize sizeToContentAutoSizeMaximumSize() const;
    void setShouldExpandToViewHeightForAutoLayout(bool);
    bool shouldExpandToViewHeightForAutoLayout() const;
    void setIntrinsicContentSize(CGSize);
    CGSize intrinsicContentSize() const;

    void setViewScale(CGFloat);
    CGFloat viewScale() const;

    void showSafeBrowsingWarning(const SafeBrowsingWarning&, CompletionHandler<void(std::variant<ContinueUnsafeLoad, URL>&&)>&&);
    void clearSafeBrowsingWarning();
    void clearSafeBrowsingWarningIfForMainFrameNavigation();

    WKLayoutMode layoutMode() const;
    void setLayoutMode(WKLayoutMode);
    void updateSupportsArbitraryLayoutModes();

    void windowDidOrderOffScreen();
    void windowDidOrderOnScreen();
    void windowDidBecomeKey(NSWindow *);
    void windowDidResignKey(NSWindow *);
    void windowDidMiniaturize();
    void windowDidDeminiaturize();
    void windowDidMove();
    void windowDidResize();
    void windowDidChangeBackingProperties(CGFloat oldBackingScaleFactor);
    void windowDidChangeScreen();
    void windowDidChangeLayerHosting();
    void windowDidChangeOcclusionState();
    void screenDidChangeColorSpace();
    bool shouldDelayWindowOrderingForEvent(NSEvent *);
    bool windowResizeMouseLocationIsInVisibleScrollerThumb(CGPoint);

    void accessibilitySettingsDidChange();

    // -[NSView mouseDownCanMoveWindow] returns YES when the NSView is transparent,
    // but we don't want a drag in the NSView to move the window, even if it's transparent.
    static bool mouseDownCanMoveWindow() { return false; }

    void viewWillMoveToWindow(NSWindow *);
    void viewDidMoveToWindow();
    void viewDidChangeBackingProperties();
    void viewDidHide();
    void viewDidUnhide();
    void activeSpaceDidChange();

    void pageDidScroll(const WebCore::IntPoint&);

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)
    NSRect scrollViewFrame();
    bool hasScrolledContentsUnderTitlebar();
    void updateTitlebarAdjacencyState();
#endif

    NSView *hitTest(CGPoint);

    WebCore::DestinationColorSpace colorSpace();

    void setUnderlayColor(NSColor *);
    RetainPtr<NSColor> underlayColor() const;
    RetainPtr<NSColor> pageExtendedBackgroundColor() const;
    
    _WKRectEdge pinnedState();
    _WKRectEdge rubberBandingEnabled();
    void setRubberBandingEnabled(_WKRectEdge);

    void setOverlayScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle);
    std::optional<WebCore::ScrollbarOverlayStyle> overlayScrollbarStyle() const;

    void beginDeferringViewInWindowChanges();
    // FIXME: Merge these two?
    void endDeferringViewInWindowChanges();
    void endDeferringViewInWindowChangesSync();
    bool isDeferringViewInWindowChanges() const { return m_shouldDeferViewInWindowChanges; }

    void setWindowOcclusionDetectionEnabled(bool enabled) { m_windowOcclusionDetectionEnabled = enabled; }
    bool windowOcclusionDetectionEnabled() const { return m_windowOcclusionDetectionEnabled; }

    void prepareForMoveToWindow(NSWindow *targetWindow, WTF::Function<void()>&& completionHandler);
    NSWindow *targetWindowForMovePreparation() const { return m_targetWindowForMovePreparation.get(); }

    void setFontForWebView(NSFont *, id);

    void updateSecureInputState();
    void resetSecureInputState();
    bool inSecureInputState() const { return m_inSecureInputState; }
    void notifyInputContextAboutDiscardedComposition();
    
    void handleAcceptedAlternativeText(const String&);
    NSInteger spellCheckerDocumentTag();

    void pressureChangeWithEvent(NSEvent *);
    NSEvent *lastPressureEvent() { return m_lastPressureEvent.get(); }

#if ENABLE(FULLSCREEN_API)
    bool hasFullScreenWindowController() const;
    WKFullScreenWindowController *fullScreenWindowController();
    void closeFullScreenWindowController();
#endif
    NSView *fullScreenPlaceholderView();
    NSWindow *fullScreenWindow();

    bool isEditable() const;
    bool executeSavedCommandBySelector(SEL);
    void executeEditCommandForSelector(SEL, const String& argument = String());
    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo);
    void clearAllEditCommands();
    bool writeSelectionToPasteboard(NSPasteboard *, NSArray *types);
    bool readSelectionFromPasteboard(NSPasteboard *);
    id validRequestorForSendAndReturnTypes(NSString *sendType, NSString *returnType);
    void centerSelectionInVisibleArea();
    void selectionDidChange();
    
    void didBecomeEditable();
    void updateFontManagerIfNeeded();
    void changeFontFromFontManager();
    void changeFontAttributesFromSender(id);
    void changeFontColorFromSender(id);
    bool validateUserInterfaceItem(id <NSValidatedUserInterfaceItem>);
    void setEditableElementIsFocused(bool);

    void startSpeaking();
    void stopSpeaking(id);

    void showGuessPanel(id);
    void checkSpelling();
    void changeSpelling(id);

    void setContinuousSpellCheckingEnabled(bool);
    void toggleContinuousSpellChecking();

    bool isGrammarCheckingEnabled();
    void setGrammarCheckingEnabled(bool);
    void toggleGrammarChecking();
    void toggleAutomaticSpellingCorrection();
    void orderFrontSubstitutionsPanel(id);
    void toggleSmartInsertDelete();
    bool isAutomaticQuoteSubstitutionEnabled();
    void setAutomaticQuoteSubstitutionEnabled(bool);
    void toggleAutomaticQuoteSubstitution();
    bool isAutomaticDashSubstitutionEnabled();
    void setAutomaticDashSubstitutionEnabled(bool);
    void toggleAutomaticDashSubstitution();
    bool isAutomaticLinkDetectionEnabled();
    void setAutomaticLinkDetectionEnabled(bool);
    void toggleAutomaticLinkDetection();
    bool isAutomaticTextReplacementEnabled();
    void setAutomaticTextReplacementEnabled(bool);
    void toggleAutomaticTextReplacement();
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();

    void requestCandidatesForSelectionIfNeeded();

    void preferencesDidChange();

    void setTextIndicator(WebCore::TextIndicator&, WebCore::TextIndicatorLifetime = WebCore::TextIndicatorLifetime::Permanent);
    void clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation);
    void setTextIndicatorAnimationProgress(float);
    void dismissContentRelativeChildWindowsFromViewOnly();
    void dismissContentRelativeChildWindowsWithAnimation(bool);
    void dismissContentRelativeChildWindowsWithAnimationFromViewOnly(bool);
    static void hideWordDefinitionWindow();

    void quickLookWithEvent(NSEvent *);
    void prepareForDictionaryLookup();
    void setAllowsLinkPreview(bool);
    bool allowsLinkPreview() const { return m_allowsLinkPreview; }
    NSObject *immediateActionAnimationControllerForHitTestResult(API::HitTestResult*, uint32_t type, API::Object* userData);
    void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object* userData);
    void prepareForImmediateActionAnimation();
    void cancelImmediateActionAnimation();
    void completeImmediateActionAnimation();
    void didChangeContentSize(CGSize);
    void didHandleAcceptedCandidate();
    void videoControlsManagerDidChange();

    void setIgnoresNonWheelEvents(bool);
    bool ignoresNonWheelEvents() const { return m_ignoresNonWheelEvents; }
    void setIgnoresMouseMoveEvents(bool ignoresMouseMoveEvents) { m_ignoresMouseMoveEvents = ignoresMouseMoveEvents; }
    bool ignoresMouseMoveEvents() const { return m_ignoresMouseMoveEvents; }
    void setIgnoresAllEvents(bool);
    bool ignoresAllEvents() const { return m_ignoresAllEvents; }
    void setIgnoresMouseDraggedEvents(bool);
    bool ignoresMouseDraggedEvents() const { return m_ignoresMouseDraggedEvents; }

    void setAccessibilityWebProcessToken(NSData *);
    void accessibilityRegisterUIProcessTokens();
    void updateRemoteAccessibilityRegistration(bool registerProcess);
    id accessibilityFocusedUIElement();
    bool accessibilityIsIgnored() const { return false; }
    id accessibilityHitTest(CGPoint);
    void enableAccessibilityIfNecessary();
    id accessibilityAttributeValue(NSString *, id parameter = nil);

    NSTrackingArea *primaryTrackingArea() const { return m_primaryTrackingArea.get(); }
    void updatePrimaryTrackingAreaOptions(NSTrackingAreaOptions);

    NSTrackingRectTag addTrackingRect(CGRect, id owner, void* userData, bool assumeInside);
    NSTrackingRectTag addTrackingRectWithTrackingNum(CGRect, id owner, void* userData, bool assumeInside, int tag);
    void addTrackingRectsWithTrackingNums(CGRect*, id owner, void** userDataList, bool assumeInside, NSTrackingRectTag *trackingNums, int count);
    void removeTrackingRect(NSTrackingRectTag);
    void removeTrackingRects(NSTrackingRectTag *, int count);
    NSString *stringForToolTip(NSToolTipTag tag);
    void toolTipChanged(const String& oldToolTip, const String& newToolTip);

    void enterAcceleratedCompositingWithRootLayer(CALayer *);
    void setAcceleratedCompositingRootLayer(CALayer *);
    CALayer *acceleratedCompositingRootLayer() const { return m_rootLayer.get(); }

    void setThumbnailView(_WKThumbnailView *);
    _WKThumbnailView *thumbnailView() const { return m_thumbnailView; }

    void setInspectorAttachmentView(NSView *);
    NSView *inspectorAttachmentView();
    
    void showShareSheet(const WebCore::ShareDataWithParsedURL&, WTF::CompletionHandler<void(bool)>&&, WKWebView *);
    void shareSheetDidDismiss(WKShareSheet *);

    _WKRemoteObjectRegistry *remoteObjectRegistry();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKBrowsingContextController *browsingContextController();
    ALLOW_DEPRECATED_DECLARATIONS_END

#if ENABLE(DRAG_SUPPORT)
    void draggedImage(NSImage *, CGPoint endPoint, NSDragOperation);
    NSDragOperation draggingEntered(id <NSDraggingInfo>);
    NSDragOperation draggingUpdated(id <NSDraggingInfo>);
    void draggingExited(id <NSDraggingInfo>);
    bool prepareForDragOperation(id <NSDraggingInfo>);
    bool performDragOperation(id <NSDraggingInfo>);
    NSView *hitTestForDragTypes(CGPoint, NSSet *types);
    void registerDraggedTypes();

    NSDragOperation dragSourceOperationMask(NSDraggingSession *, NSDraggingContext);
    void draggingSessionEnded(NSDraggingSession *, NSPoint, NSDragOperation);

    NSString *fileNameForFilePromiseProvider(NSFilePromiseProvider *, NSString *fileType);
    void writeToURLForFilePromiseProvider(NSFilePromiseProvider *, NSURL *, void(^)(NSError *));

    void didPerformDragOperation(bool handled);
#endif

    void startWindowDrag();

    void startDrag(const WebCore::DragItem&, const ShareableBitmapHandle& image);
    void setFileAndURLTypes(NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, NSPasteboard *);
    void setPromisedDataForImage(WebCore::Image*, NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, WebCore::FragmentedSharedBuffer* archiveBuffer, NSString *pasteboardName, NSString *pasteboardOrigin);
    void pasteboardChangedOwner(NSPasteboard *);
    void provideDataForPasteboard(NSPasteboard *, NSString *type);
    NSArray *namesOfPromisedFilesDroppedAtDestination(NSURL *dropDestination);

    RefPtr<ViewSnapshot> takeViewSnapshot();
    void saveBackForwardSnapshotForCurrentItem();
    void saveBackForwardSnapshotForItem(WebBackForwardListItem&);

    WKSafeBrowsingWarning *safeBrowsingWarning() { return m_safeBrowsingWarning.get(); }

    ViewGestureController* gestureController() { return m_gestureController.get(); }
    ViewGestureController& ensureGestureController();
    void setAllowsBackForwardNavigationGestures(bool);
    bool allowsBackForwardNavigationGestures() const { return m_allowsBackForwardNavigationGestures; }
    void setAllowsMagnification(bool);
    bool allowsMagnification() const { return m_allowsMagnification; }

    void setMagnification(double, CGPoint centerPoint);
    void setMagnification(double);
    double magnification() const;
    void setCustomSwipeViews(NSArray *);
    void setCustomSwipeViewsTopContentInset(float);
    bool tryToSwipeWithEvent(NSEvent *, bool ignoringPinnedState);
    void setDidMoveSwipeSnapshotCallback(BlockPtr<void (CGRect)>&&);

    void scrollWheel(NSEvent *);
    void swipeWithEvent(NSEvent *);
    void magnifyWithEvent(NSEvent *);
    void rotateWithEvent(NSEvent *);
    void smartMagnifyWithEvent(NSEvent *);

    RetainPtr<NSEvent> setLastMouseDownEvent(NSEvent *);

    void gestureEventWasNotHandledByWebCore(NSEvent *);
    void gestureEventWasNotHandledByWebCoreFromViewOnly(NSEvent *);

    void didRestoreScrollPosition();
    
    void scrollToRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);

    void setTotalHeightOfBanners(CGFloat totalHeightOfBanners) { m_totalHeightOfBanners = totalHeightOfBanners; }
    CGFloat totalHeightOfBanners() const { return m_totalHeightOfBanners; }

    void doneWithKeyEvent(NSEvent *, bool eventWasHandled);
    NSArray *validAttributesForMarkedText();
    void doCommandBySelector(SEL);
    void insertText(id string);
    void insertText(id string, NSRange replacementRange);
    NSTextInputContext *inputContext();
    void unmarkText();
    void setMarkedText(id string, NSRange selectedRange, NSRange replacementRange);
    NSRange selectedRange();
    bool hasMarkedText();
    NSRange markedRange();
    NSAttributedString *attributedSubstringForProposedRange(NSRange, NSRangePointer actualRange);
    NSUInteger characterIndexForPoint(NSPoint);
    NSRect firstRectForCharacterRange(NSRange, NSRangePointer actualRange);
    bool performKeyEquivalent(NSEvent *);
    void keyUp(NSEvent *);
    void keyDown(NSEvent *);
    void flagsChanged(NSEvent *);

    // Override this so that AppKit will send us arrow keys as key down events so we can
    // support them via the key bindings mechanism.
    static bool wantsKeyDownForEvent(NSEvent *) { return true; }

    void selectedRangeWithCompletionHandler(void(^)(NSRange));
    void hasMarkedTextWithCompletionHandler(void(^)(BOOL hasMarkedText));
    void markedRangeWithCompletionHandler(void(^)(NSRange));
    void attributedSubstringForProposedRange(NSRange, void(^)(NSAttributedString *attrString, NSRange actualRange));
    void firstRectForCharacterRange(NSRange, void(^)(NSRect firstRect, NSRange actualRange));
    void characterIndexForPoint(NSPoint, void(^)(NSUInteger));
    void typingAttributesWithCompletionHandler(void(^)(NSDictionary<NSString *, id> *));

    void mouseMoved(NSEvent *);
    void mouseDown(NSEvent *);
    void mouseUp(NSEvent *);
    void mouseDragged(NSEvent *);
    void mouseEntered(NSEvent *);
    void mouseExited(NSEvent *);
    void otherMouseDown(NSEvent *);
    void otherMouseDragged(NSEvent *);
    void otherMouseUp(NSEvent *);
    void rightMouseDown(NSEvent *);
    void rightMouseDragged(NSEvent *);
    void rightMouseUp(NSEvent *);

    void forceRequestCandidatesForTesting();
    bool shouldRequestCandidates() const;

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(const URL& imageURL, const ShareableBitmapHandle& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&);
    void computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    WebCore::FloatRect imageAnalysisInteractionBounds() const { return m_imageAnalysisInteractionBounds; }
    VKCImageAnalysisOverlayView *imageAnalysisOverlayView() const { return m_imageAnalysisOverlayView.get(); }
#endif

    bool imageAnalysisOverlayViewHasCursorAtPoint(NSPoint locationInView) const;

    bool acceptsPreviewPanelControl(QLPreviewPanel *);
    void beginPreviewPanelControl(QLPreviewPanel *);
    void endPreviewPanelControl(QLPreviewPanel *);

    bool windowIsFrontWindowUnderMouse(NSEvent *);

    bool requiresUserActionForEditingControlsManager() const;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection();
    void setUserInterfaceLayoutDirection(NSUserInterfaceLayoutDirection);

    void handleAcceptedCandidate(NSTextCheckingResult *acceptedCandidate);

#if HAVE(TOUCH_BAR)
    NSTouchBar *makeTouchBar();
    void updateTouchBar();
    NSTouchBar *currentTouchBar() const { return m_currentTouchBar.get(); }
    NSCandidateListTouchBarItem *candidateListTouchBarItem() const;
#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    bool isPictureInPictureActive();
    void togglePictureInPicture();
    void updateMediaPlaybackControlsManager();

    AVTouchBarScrubber *mediaPlaybackControlsView() const;
#endif
    NSTouchBar *textTouchBar() const;
    void dismissTextTouchBarPopoverItemWithIdentifier(NSString *);

    bool clientWantsMediaPlaybackControlsView() const { return m_clientWantsMediaPlaybackControlsView; }
    void setClientWantsMediaPlaybackControlsView(bool clientWantsMediaPlaybackControlsView) { m_clientWantsMediaPlaybackControlsView = clientWantsMediaPlaybackControlsView; }

    void updateTouchBarAndRefreshTextBarIdentifiers();
    void setIsCustomizingTouchBar(bool isCustomizingTouchBar) { m_isCustomizingTouchBar = isCustomizingTouchBar; };

    bool canTogglePictureInPicture();
#endif // HAVE(TOUCH_BAR)

    bool beginBackSwipeForTesting();
    bool completeBackSwipeForTesting();
    
    void setUseSystemAppearance(bool);
    bool useSystemAppearance();

    void effectiveAppearanceDidChange();
    bool effectiveAppearanceIsDark();
    bool effectiveUserInterfaceLevelIsElevated();

    void takeFocus(WebCore::FocusDirection);
    void clearPromisedDragImage();

    void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, const WebCore::IntRect&, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&);
    void handleDOMPasteRequestForCategoryWithResult(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteAccessResponse);
    NSMenu *domPasteMenu() const { return m_domPasteMenu.get(); }
    void hideDOMPasteMenuWithResult(WebCore::DOMPasteAccessResponse);

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    bool canHandleContextMenuTranslation() const;
    void handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo&);
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionCoordinatorProxyPrivate* mediaSessionCoordinatorForTesting() { return m_coordinatorForTesting.get(); }
    void setMediaSessionCoordinatorForTesting(MediaSessionCoordinatorProxyPrivate*);
#endif

#if ENABLE(DATA_DETECTION)
    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&);
#endif

#if ENABLE(REVEAL)
    void didFinishPresentation(WKRevealItemPresenter *);
#endif

    void beginTextRecognitionForVideoInElementFullscreen(const ShareableBitmapHandle&, WebCore::FloatRect);
    void cancelTextRecognitionForVideoInElementFullscreen();

private:
#if HAVE(TOUCH_BAR)
    void setUpTextTouchBar(NSTouchBar *);
    void updateTextTouchBar();
    void updateMediaTouchBar();

    bool useMediaPlaybackControlsView() const;
    bool isRichlyEditableForTouchBar() const;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    void installImageAnalysisOverlayView(VKCImageAnalysis *);
    void uninstallImageAnalysisOverlayView();
#endif

    bool m_clientWantsMediaPlaybackControlsView { false };
    bool m_canCreateTouchBars { false };
    bool m_startedListeningToCustomizationEvents { false };
    bool m_isUpdatingTextTouchBar { false };
    bool m_isCustomizingTouchBar { false };

    RetainPtr<NSTouchBar> m_currentTouchBar;
    RetainPtr<NSTouchBar> m_richTextTouchBar;
    RetainPtr<NSTouchBar> m_plainTextTouchBar;
    RetainPtr<NSTouchBar> m_passwordTextTouchBar;
    RetainPtr<WKTextTouchBarItemController> m_textTouchBarItemController;
    RetainPtr<NSCandidateListTouchBarItem> m_richTextCandidateListTouchBarItem;
    RetainPtr<NSCandidateListTouchBarItem> m_plainTextCandidateListTouchBarItem;
    RetainPtr<NSCandidateListTouchBarItem> m_passwordTextCandidateListTouchBarItem;
    RetainPtr<WebPlaybackControlsManager> m_playbackControlsManager;
    RetainPtr<NSCustomTouchBarItem> m_exitFullScreenButton;

#if ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
    RetainPtr<AVTouchBarPlaybackControlsProvider> m_mediaTouchBarProvider;
    RetainPtr<AVTouchBarScrubber> m_mediaPlaybackControlsView;
#endif // ENABLE(WEB_PLAYBACK_CONTROLS_MANAGER)
#endif // HAVE(TOUCH_BAR)

    bool supportsArbitraryLayoutModes() const;
    float intrinsicDeviceScaleFactor() const;

    void scheduleSetTopContentInsetDispatch();
    void dispatchSetTopContentInset();

    void postFakeMouseMovedEventForFlagsChangedEvent(NSEvent *);

    void sendToolTipMouseExited();
    void sendToolTipMouseEntered();

    void reparentLayerTreeInThumbnailView();
    void updateThumbnailViewLayer();

    void setUserInterfaceItemState(NSString *commandName, bool enabled, int state);

    Vector<WebCore::KeypressCommand> collectKeyboardLayoutCommandsForEvent(NSEvent *);
    void interpretKeyEvent(NSEvent *, void(^completionHandler)(BOOL handled, const Vector<WebCore::KeypressCommand>&));

    void nativeMouseEventHandler(NSEvent *);
    void nativeMouseEventHandlerInternal(NSEvent *);
    
    void mouseMovedInternal(NSEvent *);
    void mouseDownInternal(NSEvent *);
    void mouseUpInternal(NSEvent *);
    void mouseDraggedInternal(NSEvent *);

    void handleProcessSwapOrExit();

    bool mightBeginDragWhileInactive();
    bool mightBeginScrollWhileInactive();

    void handleRequestedCandidates(NSInteger sequenceNumber, NSArray<NSTextCheckingResult *> *candidates);
    void flushPendingMouseEventCallbacks();

    void viewWillMoveToWindowImpl(NSWindow *);

    id toolTipOwnerForSendingMouseEvents() const;

#if ENABLE(DRAG_SUPPORT)
    void sendDragEndToPage(CGPoint endPoint, NSDragOperation);
#endif

#if ENABLE(IMAGE_ANALYSIS)
    CocoaImageAnalyzer *ensureImageAnalyzer();
    int32_t processImageAnalyzerRequest(CocoaImageAnalyzerRequest *, CompletionHandler<void(CocoaImageAnalysis *, NSError *)>&&);
#endif

    WeakObjCPtr<NSView<WebViewImplDelegate>> m_view;
    std::unique_ptr<PageClient> m_pageClient;
    Ref<WebPageProxy> m_page;

    DrawingAreaType m_drawingAreaType { DrawingAreaType::TiledCoreAnimation };

    bool m_willBecomeFirstResponderAgain { false };
    bool m_inBecomeFirstResponder { false };
    bool m_inResignFirstResponder { false };

    CGRect m_contentPreparationRect { { 0, 0 }, { 0, 0 } };
    bool m_useContentPreparationRectForVisibleRect { false };
    bool m_clipsToVisibleRect { false };
    bool m_needsViewFrameInWindowCoordinates;
    bool m_didScheduleWindowAndViewFrameUpdate { false };
    bool m_windowOcclusionDetectionEnabled { true };

    bool m_automaticallyAdjustsContentInsets { false };
    std::optional<CGFloat> m_pendingTopContentInset;
    bool m_didScheduleSetTopContentInsetDispatch { false };
    
    CGSize m_scrollOffsetAdjustment { 0, 0 };

    CGSize m_intrinsicContentSize { 0, 0 };

    RetainPtr<WKViewLayoutStrategy> m_layoutStrategy;
    WKLayoutMode m_lastRequestedLayoutMode { kWKLayoutModeViewSize };
    CGFloat m_lastRequestedViewScale { 1 };
    CGSize m_lastRequestedFixedLayoutSize { 0, 0 };

    bool m_inSecureInputState { false };
    RetainPtr<WKEditorUndoTarget> m_undoTarget;

    ValidationMap m_validationMap;

#if ENABLE(FULLSCREEN_API)
    RetainPtr<WKFullScreenWindowController> m_fullScreenWindowController;
#endif

#if ENABLE(UI_PROCESS_PDF_HUD)
    HashMap<WebKit::PDFPluginIdentifier, RetainPtr<WKPDFHUDView>> _pdfHUDViews;
#endif

    RetainPtr<WKShareSheet> _shareSheet;

    RetainPtr<WKWindowVisibilityObserver> m_windowVisibilityObserver;
    RetainPtr<WKAccessibilitySettingsObserver> m_accessibilitySettingsObserver;

    bool m_shouldDeferViewInWindowChanges { false };
    bool m_viewInWindowChangeWasDeferred { false };
    RetainPtr<NSWindow> m_targetWindowForMovePreparation;

    id m_flagsChangedEventMonitor { nullptr };

    std::unique_ptr<WebCore::TextIndicatorWindow> m_textIndicatorWindow;

    RetainPtr<NSColorSpace> m_colorSpace;

    RetainPtr<NSColor> m_backgroundColor;

    RetainPtr<NSEvent> m_lastMouseDownEvent;
    RetainPtr<NSEvent> m_lastPressureEvent;

    bool m_ignoresNonWheelEvents { false };
    bool m_ignoresMouseMoveEvents { false };
    bool m_ignoresAllEvents { false };
    bool m_ignoresMouseDraggedEvents { false };

    RetainPtr<WKImmediateActionController> m_immediateActionController;
    RetainPtr<NSImmediateActionGestureRecognizer> m_immediateActionGestureRecognizer;

    bool m_allowsLinkPreview { true };

    RetainPtr<WKMouseTrackingObserver> m_mouseTrackingObserver;
    RetainPtr<NSTrackingArea> m_primaryTrackingArea;

    NSToolTipTag m_lastToolTipTag { 0 };
    WeakObjCPtr<id> m_trackingRectOwner;
    void* m_trackingRectUserData { nullptr };

    RetainPtr<CALayer> m_rootLayer;
    RetainPtr<NSView> m_layerHostingView;

    _WKThumbnailView *m_thumbnailView { nullptr };

    RetainPtr<_WKRemoteObjectRegistry> m_remoteObjectRegistry;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<WKBrowsingContextController> m_browsingContextController;
    ALLOW_DEPRECATED_DECLARATIONS_END

    std::unique_ptr<ViewGestureController> m_gestureController;
    bool m_allowsBackForwardNavigationGestures { false };
    bool m_allowsMagnification { false };

    RetainPtr<NSAccessibilityRemoteUIElement> m_remoteAccessibilityChild;

    RefPtr<WebCore::Image> m_promisedImage;
    String m_promisedFilename;
    String m_promisedURL;

    CGFloat m_totalHeightOfBanners { 0 };

    RetainPtr<NSView> m_inspectorAttachmentView;

    // We keep here the event when resending it to
    // the application to distinguish the case of a new event from one
    // that has been already sent to WebCore.
    RetainPtr<NSEvent> m_keyDownEventBeingResent;
    Vector<WebCore::KeypressCommand>* m_collectedKeypressCommands { nullptr };

    String m_lastStringForCandidateRequest;
    NSInteger m_lastCandidateRequestSequenceNumber;
    NSRange m_softSpaceRange { NSNotFound, 0 };
    bool m_isHandlingAcceptedCandidate { false };
    bool m_editableElementIsFocused { false };
    bool m_isTextInsertionReplacingSoftSpace { false };
    RetainPtr<WKSafeBrowsingWarning> m_safeBrowsingWarning;
    
#if ENABLE(DRAG_SUPPORT)
    NSInteger m_initialNumberOfValidItemsForDrop { 0 };
#endif

#if HAVE(NSSCROLLVIEW_SEPARATOR_TRACKING_ADAPTER)
    bool m_pageIsScrolledToTop { true };
    bool m_isRegisteredScrollViewSeparatorTrackingAdapter { false };
    NSRect m_lastScrollViewFrame { NSZeroRect };
#endif

    RetainPtr<NSMenu> m_domPasteMenu;
    RetainPtr<WKDOMPasteMenuDelegate> m_domPasteMenuDelegate;
    CompletionHandler<void(WebCore::DOMPasteAccessResponse)> m_domPasteRequestHandler;

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<MediaSessionCoordinatorProxyPrivate> m_coordinatorForTesting;
#endif

#if ENABLE(REVEAL)
    RetainPtr<WKRevealItemPresenter> m_revealItemPresenter;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    RefPtr<WorkQueue> m_imageAnalyzerQueue;
    RetainPtr<CocoaImageAnalyzer> m_imageAnalyzer;
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    RetainPtr<VKCImageAnalysisOverlayView> m_imageAnalysisOverlayView;
    RetainPtr<WKImageAnalysisOverlayViewDelegate> m_imageAnalysisOverlayViewDelegate;
    uint32_t m_currentImageAnalysisRequestID { 0 };
    WebCore::FloatRect m_imageAnalysisInteractionBounds;
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    WeakObjCPtr<NSPopover> m_lastContextMenuTranslationPopover;
#endif
};
    
} // namespace WebKit

#endif // PLATFORM(MAC)
