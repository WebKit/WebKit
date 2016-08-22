/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "DeviceMotionClient.h"
#include "DeviceOrientationClient.h"
#include "DiagnosticLoggingClient.h"
#include "DocumentFragment.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "FloatRect.h"
#include "FocusDirection.h"
#include "FrameLoaderClient.h"
#include "InspectorClient.h"
#include "ProgressTrackerClient.h"
#include "ResourceError.h"
#include "SessionID.h"
#include "SocketProvider.h"
#include "TextCheckerClient.h"
#include "VisitedLinkStore.h"
#include <wtf/text/StringView.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "UserMessageHandlerDescriptor.h"
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "CompiledContentExtension.h"
#endif

/*
 This file holds empty Client stubs for use by WebCore.
 Viewless element needs to create a dummy Page->Frame->FrameView tree for use in parsing or executing JavaScript.
 This tree depends heavily on Clients (usually provided by WebKit classes).

 This file was first created for SVGImage as it had no way to access the current Page (nor should it,
 since Images are not tied to a page).
 See http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about this file.

 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes.  Unfortunate, yes.  Hopefully temporary.
*/

namespace WebCore {

class GraphicsContext3D;
class Page;
class PageConfiguration;

class EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EmptyChromeClient() { }
    void chromeDestroyed() override { }

    void setWindowRect(const FloatRect&) override { }
    FloatRect windowRect() override { return FloatRect(); }

    FloatRect pageRect() override { return FloatRect(); }

    void focus() override { }
    void unfocus() override { }

    bool canTakeFocus(FocusDirection) override { return false; }
    void takeFocus(FocusDirection) override { }

    void focusedElementChanged(Element*) override { }
    void focusedFrameChanged(Frame*) override { }

    Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) override { return nullptr; }
    void show() override { }

    bool canRunModal() override { return false; }
    void runModal() override { }

    void setToolbarsVisible(bool) override { }
    bool toolbarsVisible() override { return false; }

    void setStatusbarVisible(bool) override { }
    bool statusbarVisible() override { return false; }

    void setScrollbarsVisible(bool) override { }
    bool scrollbarsVisible() override { return false; }

    void setMenubarVisible(bool) override { }
    bool menubarVisible() override { return false; }

    void setResizable(bool) override { }

    void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, unsigned, const String&) override { }

    bool canRunBeforeUnloadConfirmPanel() override { return false; }
    bool runBeforeUnloadConfirmPanel(const String&, Frame*) override { return true; }

    void closeWindowSoon() override { }

    void runJavaScriptAlert(Frame*, const String&) override { }
    bool runJavaScriptConfirm(Frame*, const String&) override { return false; }
    bool runJavaScriptPrompt(Frame*, const String&, const String&, String&) override { return false; }

    bool selectItemWritingDirectionIsNatural() override { return false; }
    bool selectItemAlignmentFollowsMenuWritingDirection() override { return false; }
    bool hasOpenedPopup() const override { return false; }
    RefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const override;
    RefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const override;

    void setStatusbarText(const String&) override { }

    KeyboardUIMode keyboardUIMode() override { return KeyboardAccessDefault; }

    void invalidateRootView(const IntRect&) override { }
    void invalidateContentsAndRootView(const IntRect&) override { }
    void invalidateContentsForSlowScroll(const IntRect&) override { }
    void scroll(const IntSize&, const IntRect&, const IntRect&) override { }
#if USE(COORDINATED_GRAPHICS)
    void delegatedScrollRequested(const IntPoint&) override { }
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    void scheduleAnimation() override { }
#endif

    IntPoint screenToRootView(const IntPoint& p) const override { return p; }
    IntRect rootViewToScreen(const IntRect& r) const override { return r; }
#if PLATFORM(IOS)
    IntPoint accessibilityScreenToRootView(const IntPoint& p) const override { return p; };
    IntRect rootViewToAccessibilityScreen(const IntRect& r) const override { return r; };
#endif
    PlatformPageClient platformPageClient() const override { return 0; }
    void contentsSizeChanged(Frame*, const IntSize&) const override { }

    void scrollbarsModeDidChange() const override { }
    void mouseDidMoveOverElement(const HitTestResult&, unsigned) override { }

    void setToolTip(const String&, TextDirection) override { }

    void print(Frame*) override { }

    void exceededDatabaseQuota(Frame*, const String&, DatabaseDetails) override { }

    void reachedMaxAppCacheSize(int64_t) override { }
    void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t) override { }

#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) override;
#endif

    void runOpenPanel(Frame*, PassRefPtr<FileChooser>) override;
    void loadIconForFiles(const Vector<String>&, FileIconLoader*) override { }

    void elementDidFocus(const Node*) override { }
    void elementDidBlur(const Node*) override { }

#if !PLATFORM(IOS)
    void setCursor(const Cursor&) override { }
    void setCursorHiddenUntilMouseMoves(bool) override { }
#endif

    void scrollRectIntoView(const IntRect&) const override { }

    void attachRootGraphicsLayer(Frame*, GraphicsLayer*) override { }
    void attachViewOverlayGraphicsLayer(Frame*, GraphicsLayer*) override { }
    void setNeedsOneShotDrawingSynchronization() override { }
    void scheduleCompositingLayerFlush() override { }

#if PLATFORM(WIN)
    void setLastSetCursorToCurrentCursor() override { }
    void AXStartFrameLoad() override { }
    void AXFinishFrameLoad() override { }
#endif

#if PLATFORM(IOS)
#if ENABLE(IOS_TOUCH_EVENTS)
    void didPreventDefaultForEvent() override { }
#endif
    void didReceiveMobileDocType(bool) override { }
    void setNeedsScrollNotifications(Frame*, bool) override { }
    void observedContentChange(Frame*) override { }
    void clearContentChangeObservers(Frame*) override { }
    void notifyRevealedSelectionByScrollingFrame(Frame*) override { }
    void didLayout(LayoutType) override { }
    void didStartOverflowScroll() override { }
    void didEndOverflowScroll() override { }

    void suppressFormNotifications() override { }
    void restoreFormNotifications() override { }

    void addOrUpdateScrollingLayer(Node*, PlatformLayer*, PlatformLayer*, const IntSize&, bool, bool) override { }
    void removeScrollingLayer(Node*, PlatformLayer*, PlatformLayer*) override { }

    void webAppOrientationsUpdated() override { };
    void showPlaybackTargetPicker(bool) override { };
#endif // PLATFORM(IOS)

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const override { return 0; }
#endif

#if PLATFORM(IOS)
    bool isStopping() override { return false; }
#endif

#if ENABLE(TOUCH_EVENTS)
    void needTouchEvents(bool) override { }
#endif
    
    void wheelEventHandlersChanged(bool) override { }
    
    bool isEmptyChromeClient() const override { return true; }

    void didAssociateFormControls(const Vector<RefPtr<Element>>&) override { }
    bool shouldNotifyOnFormChanges() override { return false; }
};

// FIXME (bug 116233): Get rid of EmptyFrameLoaderClient. It is a travesty.

class EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }
    void frameLoaderDestroyed() override { }

    bool hasWebView() const override { return true; } // mainly for assertions

    void makeRepresentation(DocumentLoader*) override { }
#if PLATFORM(IOS)
    bool forceLayoutOnRestoreFromPageCache() override { return false; }
#endif
    void forceLayoutForNonHTML() override { }

    void setCopiesOnScroll() override { }

    void detachedFromParent2() override { }
    void detachedFromParent3() override { }

    void convertMainResourceLoadToDownload(DocumentLoader*, SessionID, const ResourceRequest&, const ResourceResponse&) override { }

    void assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const ResourceRequest&) override { }
    bool shouldUseCredentialStorage(DocumentLoader*, unsigned long) override { return false; }
    void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) override { }
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) override { }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long, const ProtectionSpace&) override { return false; }
#endif

#if PLATFORM(IOS)
    RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, unsigned long) override { return nullptr; }
#endif

    void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) override { }
    void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int) override { }
    void dispatchDidFinishLoading(DocumentLoader*, unsigned long) override { }
#if ENABLE(DATA_DETECTION)
    void dispatchDidFinishDataDetection(NSArray *) override { }
#endif
    void dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&) override { }
    bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) override { return false; }

    void dispatchDidDispatchOnloadEvents() override { }
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override { }
    void dispatchDidCancelClientRedirect() override { }
    void dispatchWillPerformClientRedirect(const URL&, double, double) override { }
    void dispatchDidChangeLocationWithinPage() override { }
    void dispatchDidPushStateWithinPage() override { }
    void dispatchDidReplaceStateWithinPage() override { }
    void dispatchDidPopStateWithinPage() override { }
    void dispatchWillClose() override { }
    void dispatchDidReceiveIcon() override { }
    void dispatchDidStartProvisionalLoad() override { }
    void dispatchDidReceiveTitle(const StringWithDirection&) override { }
    void dispatchDidCommitLoad() override { }
    void dispatchDidFailProvisionalLoad(const ResourceError&) override { }
    void dispatchDidFailLoad(const ResourceError&) override { }
    void dispatchDidFinishDocumentLoad() override { }
    void dispatchDidFinishLoad() override { }
    void dispatchDidReachLayoutMilestone(LayoutMilestones) override { }

    Frame* dispatchCreatePage(const NavigationAction&) override { return nullptr; }
    void dispatchShow() override { }

    void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, FramePolicyFunction) override { }
    void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, const String&, FramePolicyFunction) override;
    void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, FramePolicyFunction) override;
    void cancelPolicyCheck() override { }

    void dispatchUnableToImplementPolicy(const ResourceError&) override { }

    void dispatchWillSendSubmitEvent(PassRefPtr<FormState>) override;
    void dispatchWillSubmitForm(PassRefPtr<FormState>, FramePolicyFunction) override;

    void revertToProvisionalState(DocumentLoader*) override { }
    void setMainDocumentError(DocumentLoader*, const ResourceError&) override { }

    void setMainFrameDocumentReady(bool) override { }

    void startDownload(const ResourceRequest&, const String& suggestedName = String()) override { UNUSED_PARAM(suggestedName); }

    void willChangeTitle(DocumentLoader*) override { }
    void didChangeTitle(DocumentLoader*) override { }

    void willReplaceMultipartContent() override { }
    void didReplaceMultipartContent() override { }

    void committedLoad(DocumentLoader*, const char*, int) override { }
    void finishedLoading(DocumentLoader*) override { }

    ResourceError cancelledError(const ResourceRequest&) override { return ResourceError(ResourceError::Type::Cancellation); }
    ResourceError blockedError(const ResourceRequest&) override { return { }; }
    ResourceError blockedByContentBlockerError(const ResourceRequest&) override { return { }; }
    ResourceError cannotShowURLError(const ResourceRequest&) override { return { }; }
    ResourceError interruptedForPolicyChangeError(const ResourceRequest&) override { return { }; }
#if ENABLE(CONTENT_FILTERING)
    ResourceError blockedByContentFilterError(const ResourceRequest&) override { return { }; }
#endif

    ResourceError cannotShowMIMETypeError(const ResourceResponse&) override { return { }; }
    ResourceError fileDoesNotExistError(const ResourceResponse&) override { return { }; }
    ResourceError pluginWillHandleLoadError(const ResourceResponse&) override { return { }; }

    bool shouldFallBack(const ResourceError&) override { return false; }

    bool canHandleRequest(const ResourceRequest&) const override { return false; }
    bool canShowMIMEType(const String&) const override { return false; }
    bool canShowMIMETypeAsHTML(const String&) const override { return false; }
    bool representationExistsForURLScheme(const String&) const override { return false; }
    String generatedMIMETypeForURLScheme(const String&) const override { return emptyString(); }

    void frameLoadCompleted() override { }
    void restoreViewState() override { }
    void provisionalLoadStarted() override { }
    void didFinishLoad() override { }
    void prepareForDataSourceReplacement() override { }

    Ref<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) override;
    void updateCachedDocumentLoader(DocumentLoader&) override { }
    void setTitle(const StringWithDirection&, const URL&) override { }

    String userAgent(const URL&) override { return emptyString(); }

    void savePlatformDataToCachedFrame(CachedFrame*) override { }
    void transitionToCommittedFromCachedFrame(CachedFrame*) override { }
#if PLATFORM(IOS)
    void didRestoreFrameHierarchyForCachedFrame() override { }
#endif
    void transitionToCommittedForNewPage() override { }

    void didSaveToPageCache() override { }
    void didRestoreFromPageCache() override { }

    void dispatchDidBecomeFrameset(bool) override { }

    void updateGlobalHistory() override { }
    void updateGlobalHistoryRedirectLinks() override { }
    bool shouldGoToHistoryItem(HistoryItem*) const override { return false; }
    void updateGlobalHistoryItemForPage() override { }
    void saveViewStateToItem(HistoryItem&) override { }
    bool canCachePage() const override { return false; }
    void didDisplayInsecureContent() override { }
    void didRunInsecureContent(SecurityOrigin*, const URL&) override { }
    void didDetectXSS(const URL&, bool) override { }
    RefPtr<Frame> createFrame(const URL&, const String&, HTMLFrameOwnerElement*, const String&, bool, int, int) override;
    RefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool) override;
    void recreatePlugin(Widget*) override;
    PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const URL&, const Vector<String>&, const Vector<String>&) override;

    ObjectContentType objectContentType(const URL&, const String&) override { return ObjectContentType::None; }
    String overrideMediaType() const override { return String(); }

    void redirectDataToPlugin(Widget*) override { }
    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) override { }

    void registerForIconNotification(bool) override { }

#if PLATFORM(COCOA)
    RemoteAXObjectRef accessibilityRemoteObject() override { return nullptr; }
    NSCachedURLResponse* willCacheResponse(DocumentLoader*, unsigned long, NSCachedURLResponse* response) const override { return response; }
#endif
#if PLATFORM(WIN) && USE(CFNETWORK)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    bool shouldCacheResponse(DocumentLoader*, unsigned long, const ResourceResponse&, const unsigned char*, unsigned long long) override { return true; }
#endif

    PassRefPtr<FrameNetworkingContext> createNetworkingContext() override;

#if ENABLE(REQUEST_AUTOCOMPLETE)
    void didRequestAutocomplete(PassRefPtr<FormState>) override { }
#endif

    bool isEmptyFrameLoaderClient() override { return true; }

    void prefetchDNS(const String&) override { }
};

class EmptyTextCheckerClient : public TextCheckerClient {
public:
    bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const override { return true; }
    void ignoreWordInSpellDocument(const String&) override { }
    void learnWord(const String&) override { }
    void checkSpellingOfString(StringView, int*, int*) override { }
    String getAutoCorrectSuggestionForMisspelledWord(const String&) override { return String(); }
    void checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*) override { }

#if USE(UNIFIED_TEXT_CHECKING)
    Vector<TextCheckingResult> checkTextOfParagraph(StringView, TextCheckingTypeMask, const VisibleSelection&) override { return Vector<TextCheckingResult>(); }
#endif

    void getGuessesForWord(const String&, const String&, const VisibleSelection&, Vector<String>&) override { }
    void requestCheckingOfString(PassRefPtr<TextCheckingRequest>, const VisibleSelection&) override;
};

class EmptyEditorClient : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }

    bool shouldDeleteRange(Range*) override { return false; }
    bool smartInsertDeleteEnabled() override { return false; }
    bool isSelectTrailingWhitespaceEnabled() override { return false; }
    bool isContinuousSpellCheckingEnabled() override { return false; }
    void toggleContinuousSpellChecking() override { }
    bool isGrammarCheckingEnabled() override { return false; }
    void toggleGrammarChecking() override { }
    int spellCheckerDocumentTag() override { return -1; }


    bool shouldBeginEditing(Range*) override { return false; }
    bool shouldEndEditing(Range*) override { return false; }
    bool shouldInsertNode(Node*, Range*, EditorInsertAction) override { return false; }
    bool shouldInsertText(const String&, Range*, EditorInsertAction) override { return false; }
    bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) override { return false; }

    bool shouldApplyStyle(StyleProperties*, Range*) override { return false; }
    void didApplyStyle() override { }
    bool shouldMoveRangeAfterDelete(Range*, Range*) override { return false; }

    void didBeginEditing() override { }
    void respondToChangedContents() override { }
    void respondToChangedSelection(Frame*) override { }
    void didChangeSelectionAndUpdateLayout() override { }
    void discardedComposition(Frame*) override { }
    void didEndEditing() override { }
    void willWriteSelectionToPasteboard(Range*) override { }
    void didWriteSelectionToPasteboard() override { }
    void getClientPasteboardDataForRange(Range*, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) override { }
    void requestCandidatesForSelection(const VisibleSelection&) override { }
    void handleAcceptedCandidateWithSoftSpaces(TextCheckingResult) override { }

    void registerUndoStep(PassRefPtr<UndoStep>) override;
    void registerRedoStep(PassRefPtr<UndoStep>) override;
    void clearUndoRedoOperations() override { }

    bool canCopyCut(Frame*, bool defaultValue) const override { return defaultValue; }
    bool canPaste(Frame*, bool defaultValue) const override { return defaultValue; }
    bool canUndo() const override { return false; }
    bool canRedo() const override { return false; }

    void undo() override { }
    void redo() override { }

    void handleKeyboardEvent(KeyboardEvent*) override { }
    void handleInputMethodKeydown(KeyboardEvent*) override { }

    void textFieldDidBeginEditing(Element*) override { }
    void textFieldDidEndEditing(Element*) override { }
    void textDidChangeInTextField(Element*) override { }
    bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) override { return false; }
    void textWillBeDeletedInTextField(Element*) override { }
    void textDidChangeInTextArea(Element*) override { }
    void overflowScrollPositionChanged() override { }

#if PLATFORM(IOS)
    void startDelayingAndCoalescingContentChangeNotifications() override { }
    void stopDelayingAndCoalescingContentChangeNotifications() override { }
    void writeDataToPasteboard(NSDictionary*) override { }
    NSArray* supportedPasteboardTypesForCurrentSelection() override { return nullptr; }
    NSArray* readDataFromPasteboard(NSString*, int) override { return nullptr; }
    bool hasRichlyEditableSelection() override { return false; }
    int getPasteboardItemsCount() override { return 0; }
    RefPtr<DocumentFragment> documentFragmentFromDelegate(int) override { return nullptr; }
    bool performsTwoStepPaste(DocumentFragment*) override { return false; }
    int pasteboardChangeCount() override { return 0; }
#endif

#if PLATFORM(COCOA)
    NSString *userVisibleString(NSURL *) override { return nullptr; }
    void setInsertionPasteboard(const String&) override { };
    NSURL *canonicalizeURL(NSURL *) override { return nullptr; }
    NSURL *canonicalizeURLString(NSString *) override { return nullptr; }
#endif

#if USE(APPKIT)
    void uppercaseWord() override { }
    void lowercaseWord() override { }
    void capitalizeWord() override { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool) override { }
    bool substitutionsPanelIsShowing() override { return false; }
    void toggleSmartInsertDelete() override { }
    bool isAutomaticQuoteSubstitutionEnabled() override { return false; }
    void toggleAutomaticQuoteSubstitution() override { }
    bool isAutomaticLinkDetectionEnabled() override { return false; }
    void toggleAutomaticLinkDetection() override { }
    bool isAutomaticDashSubstitutionEnabled() override { return false; }
    void toggleAutomaticDashSubstitution() override { }
    bool isAutomaticTextReplacementEnabled() override { return false; }
    void toggleAutomaticTextReplacement() override { }
    bool isAutomaticSpellingCorrectionEnabled() override { return false; }
    void toggleAutomaticSpellingCorrection() override { }
#endif

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() override { return false; }
#endif
    TextCheckerClient* textChecker() override { return &m_textCheckerClient; }

    void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) override { }
    void updateSpellingUIWithMisspelledWord(const String&) override { }
    void showSpellingUI(bool) override { }
    bool spellingUIIsShowing() override { return false; }

    void willSetInputMethodState() override { }
    void setInputMethodState(bool) override { }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

#if ENABLE(CONTEXT_MENUS)
class EmptyContextMenuClient : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    void contextMenuDestroyed() override { }

    void downloadURL(const URL&) override { }
    void searchWithGoogle(const Frame*) override { }
    void lookUpInDictionary(Frame*) override { }
    bool isSpeaking() override { return false; }
    void speak(const String&) override { }
    void stopSpeaking() override { }

#if PLATFORM(COCOA)
    void searchWithSpotlight() override { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    void showContextMenu() override { }
#endif
};
#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(DRAG_SUPPORT)
class EmptyDragClient : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    void willPerformDragDestinationAction(DragDestinationAction, DragData&) override { }
    void willPerformDragSourceAction(DragSourceAction, const IntPoint&, DataTransfer&) override { }
    DragDestinationAction actionMaskForDrag(DragData&) override { return DragDestinationActionNone; }
    DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) override { return DragSourceActionNone; }
    void startDrag(DragImageRef, const IntPoint&, const IntPoint&, DataTransfer&, Frame&, bool) override { }
    void dragControllerDestroyed() override { }
};
#endif // ENABLE(DRAG_SUPPORT)

class EmptyInspectorClient : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    void inspectedPageDestroyed() override { }
    
    Inspector::FrontendChannel* openLocalFrontend(InspectorController*) override { return nullptr; }
    void bringFrontendToFront() override { }

    void highlight() override { }
    void hideHighlight() override { }
};

class EmptyDeviceClient : public DeviceClient {
public:
    void startUpdating() override { }
    void stopUpdating() override { }
};

class EmptyDeviceMotionClient : public DeviceMotionClient {
public:
    void setController(DeviceMotionController*) override { }
    DeviceMotionData* lastMotion() const override { return nullptr; }
    void deviceMotionControllerDestroyed() override { }
};

class EmptyDeviceOrientationClient : public DeviceOrientationClient {
public:
    void setController(DeviceOrientationController*) override { }
    DeviceOrientationData* lastOrientation() const override { return nullptr; }
    void deviceOrientationControllerDestroyed() override { }
};

class EmptyProgressTrackerClient : public ProgressTrackerClient {
    void willChangeEstimatedProgress() override { }
    void didChangeEstimatedProgress() override { }

    void progressStarted(Frame&) override { }
    void progressEstimateChanged(Frame&) override { }
    void progressFinished(Frame&) override { }
};

class EmptyDiagnosticLoggingClient final : public DiagnosticLoggingClient {
    void logDiagnosticMessage(const String&, const String&, ShouldSample) override { }
    void logDiagnosticMessageWithResult(const String&, const String&, DiagnosticLoggingResultType, ShouldSample) override { }
    void logDiagnosticMessageWithValue(const String&, const String&, const String&, ShouldSample) override { }
};

void fillWithEmptyClients(PageConfiguration&);

}
