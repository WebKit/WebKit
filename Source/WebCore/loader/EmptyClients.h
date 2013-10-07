/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EmptyClients_h
#define EmptyClients_h

#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "DeviceMotionClient.h"
#include "DeviceOrientationClient.h"
#include "DragClient.h"
#include "EditorClient.h"
#include "TextCheckerClient.h"
#include "FloatRect.h"
#include "FocusDirection.h"
#include "FrameLoaderClient.h"
#include "InspectorClient.h"
#include "Page.h"
#include "ResourceError.h"

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

class EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EmptyChromeClient() { }
    virtual void chromeDestroyed() OVERRIDE { }

    virtual void setWindowRect(const FloatRect&) OVERRIDE { }
    virtual FloatRect windowRect() OVERRIDE { return FloatRect(); }

    virtual FloatRect pageRect() OVERRIDE { return FloatRect(); }

    virtual void focus() OVERRIDE { }
    virtual void unfocus() OVERRIDE { }

    virtual bool canTakeFocus(FocusDirection) OVERRIDE { return false; }
    virtual void takeFocus(FocusDirection) OVERRIDE { }

    virtual void focusedElementChanged(Element*) OVERRIDE { }
    virtual void focusedFrameChanged(Frame*) OVERRIDE { }

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) OVERRIDE { return 0; }
    virtual void show() OVERRIDE { }

    virtual bool canRunModal() OVERRIDE { return false; }
    virtual void runModal() OVERRIDE { }

    virtual void setToolbarsVisible(bool) OVERRIDE { }
    virtual bool toolbarsVisible() OVERRIDE { return false; }

    virtual void setStatusbarVisible(bool) OVERRIDE { }
    virtual bool statusbarVisible() OVERRIDE { return false; }

    virtual void setScrollbarsVisible(bool) OVERRIDE { }
    virtual bool scrollbarsVisible() OVERRIDE { return false; }

    virtual void setMenubarVisible(bool) OVERRIDE { }
    virtual bool menubarVisible() OVERRIDE { return false; }

    virtual void setResizable(bool) OVERRIDE { }

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, unsigned, const String&) OVERRIDE { }

    virtual bool canRunBeforeUnloadConfirmPanel() OVERRIDE { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, Frame*) OVERRIDE { return true; }

    virtual void closeWindowSoon() OVERRIDE { }

    virtual void runJavaScriptAlert(Frame*, const String&) OVERRIDE { }
    virtual bool runJavaScriptConfirm(Frame*, const String&) OVERRIDE { return false; }
    virtual bool runJavaScriptPrompt(Frame*, const String&, const String&, String&) OVERRIDE { return false; }
    virtual bool shouldInterruptJavaScript() OVERRIDE { return false; }

    virtual bool selectItemWritingDirectionIsNatural() OVERRIDE { return false; }
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() OVERRIDE { return false; }
    virtual bool hasOpenedPopup() const OVERRIDE { return false; }
    virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const OVERRIDE;
    virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const OVERRIDE;

    virtual void setStatusbarText(const String&) OVERRIDE { }

    virtual KeyboardUIMode keyboardUIMode() OVERRIDE { return KeyboardAccessDefault; }

    virtual IntRect windowResizerRect() const OVERRIDE { return IntRect(); }

    virtual void invalidateRootView(const IntRect&, bool) OVERRIDE { }
    virtual void invalidateContentsAndRootView(const IntRect&, bool) OVERRIDE { }
    virtual void invalidateContentsForSlowScroll(const IntRect&, bool) OVERRIDE { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) OVERRIDE { }
#if USE(TILED_BACKING_STORE)
    virtual void delegatedScrollRequested(const IntPoint&) { }
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    virtual void scheduleAnimation() { }
#endif

    virtual IntPoint screenToRootView(const IntPoint& p) const OVERRIDE { return p; }
    virtual IntRect rootViewToScreen(const IntRect& r) const OVERRIDE { return r; }
    virtual PlatformPageClient platformPageClient() const OVERRIDE { return 0; }
    virtual void contentsSizeChanged(Frame*, const IntSize&) const OVERRIDE { }

    virtual void scrollbarsModeDidChange() const OVERRIDE { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned) OVERRIDE { }

    virtual void setToolTip(const String&, TextDirection) OVERRIDE { }

    virtual void print(Frame*) OVERRIDE { }

#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(Frame*, const String&, DatabaseDetails) OVERRIDE { }
#endif

    virtual void reachedMaxAppCacheSize(int64_t) OVERRIDE { }
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t) OVERRIDE { }

#if ENABLE(DIRECTORY_UPLOAD)
    virtual void enumerateChosenDirectory(FileChooser*) { }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) OVERRIDE;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) OVERRIDE;
#endif

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) OVERRIDE;
    virtual void loadIconForFiles(const Vector<String>&, FileIconLoader*) OVERRIDE { }

    virtual void formStateDidChange(const Node*) OVERRIDE { }

    virtual void elementDidFocus(const Node*) OVERRIDE { }
    virtual void elementDidBlur(const Node*) OVERRIDE { }

    virtual void setCursor(const Cursor&) OVERRIDE { }
    virtual void setCursorHiddenUntilMouseMoves(bool) OVERRIDE { }

    virtual void scrollRectIntoView(const IntRect&) const OVERRIDE { }

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) OVERRIDE { }
    virtual void setNeedsOneShotDrawingSynchronization() OVERRIDE { }
    virtual void scheduleCompositingLayerFlush() OVERRIDE { }
#endif

#if PLATFORM(WIN)
    virtual void setLastSetCursorToCurrentCursor() OVERRIDE { }
    virtual void AXStartFrameLoad() OVERRIDE { }
    virtual void AXFinishFrameLoad() OVERRIDE { }
#endif
#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) OVERRIDE { }
#endif
    
    virtual void numWheelEventHandlersChanged(unsigned) OVERRIDE { }
    
    virtual bool isEmptyChromeClient() const OVERRIDE { return true; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element>>&) OVERRIDE { }
    virtual bool shouldNotifyOnFormChanges() OVERRIDE { return false; }
};

// FIXME (bug 116233): Get rid of EmptyFrameLoaderClient. It is a travesty.

class EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() OVERRIDE { }

    virtual bool hasWebView() const OVERRIDE { return true; } // mainly for assertions

    virtual void makeRepresentation(DocumentLoader*) OVERRIDE { }
    virtual void forceLayout() OVERRIDE { }
    virtual void forceLayoutForNonHTML() OVERRIDE { }

    virtual void setCopiesOnScroll() OVERRIDE { }

    virtual void detachedFromParent2() OVERRIDE { }
    virtual void detachedFromParent3() OVERRIDE { }

    virtual void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) OVERRIDE { }

    virtual void assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const ResourceRequest&) OVERRIDE { }
    virtual bool shouldUseCredentialStorage(DocumentLoader*, unsigned long) OVERRIDE { return false; }
    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) OVERRIDE { }
    virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) OVERRIDE { }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long, const ProtectionSpace&) OVERRIDE { return false; }
#endif
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int) OVERRIDE { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) OVERRIDE { }
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&) OVERRIDE { }
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) OVERRIDE { return false; }

    virtual void dispatchDidHandleOnloadEvents() OVERRIDE { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() OVERRIDE { }
    virtual void dispatchDidCancelClientRedirect() OVERRIDE { }
    virtual void dispatchWillPerformClientRedirect(const URL&, double, double) OVERRIDE { }
    virtual void dispatchDidChangeLocationWithinPage() OVERRIDE { }
    virtual void dispatchDidPushStateWithinPage() OVERRIDE { }
    virtual void dispatchDidReplaceStateWithinPage() OVERRIDE { }
    virtual void dispatchDidPopStateWithinPage() OVERRIDE { }
    virtual void dispatchWillClose() OVERRIDE { }
    virtual void dispatchDidReceiveIcon() OVERRIDE { }
    virtual void dispatchDidStartProvisionalLoad() OVERRIDE { }
    virtual void dispatchDidReceiveTitle(const StringWithDirection&) OVERRIDE { }
    virtual void dispatchDidChangeIcons(IconType) OVERRIDE { }
    virtual void dispatchDidCommitLoad() OVERRIDE { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFailLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFinishDocumentLoad() OVERRIDE { }
    virtual void dispatchDidFinishLoad() OVERRIDE { }
    virtual void dispatchDidLayout(LayoutMilestones) OVERRIDE { }

    virtual Frame* dispatchCreatePage(const NavigationAction&) OVERRIDE { return 0; }
    virtual void dispatchShow() OVERRIDE { }

    virtual void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, FramePolicyFunction) OVERRIDE { }
    virtual void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, const String&, FramePolicyFunction) OVERRIDE;
    virtual void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, FramePolicyFunction) OVERRIDE;
    virtual void cancelPolicyCheck() OVERRIDE { }

    virtual void dispatchUnableToImplementPolicy(const ResourceError&) OVERRIDE { }

    virtual void dispatchWillSendSubmitEvent(PassRefPtr<FormState>) OVERRIDE;
    virtual void dispatchWillSubmitForm(PassRefPtr<FormState>, FramePolicyFunction) OVERRIDE;

    virtual void revertToProvisionalState(DocumentLoader*) OVERRIDE { }
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) OVERRIDE { }

    virtual void willChangeEstimatedProgress() OVERRIDE { }
    virtual void didChangeEstimatedProgress() OVERRIDE { }
    virtual void postProgressStartedNotification() OVERRIDE { }
    virtual void postProgressEstimateChangedNotification() OVERRIDE { }
    virtual void postProgressFinishedNotification() OVERRIDE { }

    virtual void setMainFrameDocumentReady(bool) OVERRIDE { }

    virtual void startDownload(const ResourceRequest&, const String& suggestedName = String()) OVERRIDE { UNUSED_PARAM(suggestedName); }

    virtual void willChangeTitle(DocumentLoader*) OVERRIDE { }
    virtual void didChangeTitle(DocumentLoader*) OVERRIDE { }

    virtual void committedLoad(DocumentLoader*, const char*, int) OVERRIDE { }
    virtual void finishedLoading(DocumentLoader*) OVERRIDE { }

    virtual ResourceError cancelledError(const ResourceRequest&) OVERRIDE { ResourceError error("", 0, "", ""); error.setIsCancellation(true); return error; }
    virtual ResourceError blockedError(const ResourceRequest&) OVERRIDE { return ResourceError("", 0, "", ""); }
    virtual ResourceError cannotShowURLError(const ResourceRequest&) OVERRIDE { return ResourceError("", 0, "", ""); }
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) OVERRIDE { return ResourceError("", 0, "", ""); }

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) OVERRIDE { return ResourceError("", 0, "", ""); }
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) OVERRIDE { return ResourceError("", 0, "", ""); }
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) OVERRIDE { return ResourceError("", 0, "", ""); }

    virtual bool shouldFallBack(const ResourceError&) OVERRIDE { return false; }

    virtual bool canHandleRequest(const ResourceRequest&) const OVERRIDE { return false; }
    virtual bool canShowMIMEType(const String&) const OVERRIDE { return false; }
    virtual bool canShowMIMETypeAsHTML(const String&) const OVERRIDE { return false; }
    virtual bool representationExistsForURLScheme(const String&) const OVERRIDE { return false; }
    virtual String generatedMIMETypeForURLScheme(const String&) const OVERRIDE { return ""; }

    virtual void frameLoadCompleted() OVERRIDE { }
    virtual void restoreViewState() OVERRIDE { }
    virtual void provisionalLoadStarted() OVERRIDE { }
    virtual void didFinishLoad() OVERRIDE { }
    virtual void prepareForDataSourceReplacement() OVERRIDE { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) OVERRIDE;
    virtual void setTitle(const StringWithDirection&, const URL&) OVERRIDE { }

    virtual String userAgent(const URL&) OVERRIDE { return ""; }

    virtual void savePlatformDataToCachedFrame(CachedFrame*) OVERRIDE { }
    virtual void transitionToCommittedFromCachedFrame(CachedFrame*) OVERRIDE { }
    virtual void transitionToCommittedForNewPage() OVERRIDE { }

    virtual void didSaveToPageCache() OVERRIDE { }
    virtual void didRestoreFromPageCache() OVERRIDE { }

    virtual void dispatchDidBecomeFrameset(bool) OVERRIDE { }

    virtual void updateGlobalHistory() OVERRIDE { }
    virtual void updateGlobalHistoryRedirectLinks() OVERRIDE { }
    virtual bool shouldGoToHistoryItem(HistoryItem*) const OVERRIDE { return false; }
    virtual bool shouldStopLoadingForHistoryItem(HistoryItem*) const OVERRIDE { return false; }
    virtual void updateGlobalHistoryItemForPage() OVERRIDE { }
    virtual void saveViewStateToItem(HistoryItem*) OVERRIDE { }
    virtual bool canCachePage() const OVERRIDE { return false; }
    virtual void didDisplayInsecureContent() OVERRIDE { }
    virtual void didRunInsecureContent(SecurityOrigin*, const URL&) OVERRIDE { }
    virtual void didDetectXSS(const URL&, bool) OVERRIDE { }
    virtual PassRefPtr<Frame> createFrame(const URL&, const String&, HTMLFrameOwnerElement*, const String&, bool, int, int) OVERRIDE;
    virtual PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool) OVERRIDE;
    virtual void recreatePlugin(Widget*) OVERRIDE;
    virtual PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const URL&, const Vector<String>&, const Vector<String>&) OVERRIDE;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual PassRefPtr<Widget> createMediaPlayerProxyPlugin(const IntSize&, HTMLMediaElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&) OVERRIDE;
    virtual void hideMediaPlayerProxyPlugin(Widget*) OVERRIDE { }
    virtual void showMediaPlayerProxyPlugin(Widget*) OVERRIDE { }
#endif

    virtual ObjectContentType objectContentType(const URL&, const String&, bool) OVERRIDE { return ObjectContentType(); }
    virtual String overrideMediaType() const OVERRIDE { return String(); }

    virtual void redirectDataToPlugin(Widget*) OVERRIDE { }
    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) OVERRIDE { }
    virtual void documentElementAvailable() OVERRIDE { }
    virtual void didPerformFirstNavigation() const OVERRIDE { }

    virtual void registerForIconNotification(bool) OVERRIDE { }

#if PLATFORM(MAC)
    virtual RemoteAXObjectRef accessibilityRemoteObject() OVERRIDE { return 0; }
    virtual NSCachedURLResponse* willCacheResponse(DocumentLoader*, unsigned long, NSCachedURLResponse* response) const OVERRIDE { return response; }
#endif
#if PLATFORM(WIN) && USE(CFNETWORK)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    virtual bool shouldCacheResponse(DocumentLoader*, unsigned long, const ResourceResponse&, const unsigned char*, unsigned long long) OVERRIDE { return true; }
#endif

    virtual PassRefPtr<FrameNetworkingContext> createNetworkingContext() OVERRIDE;

    virtual bool isEmptyFrameLoaderClient() OVERRIDE { return true; }
};

class EmptyTextCheckerClient : public TextCheckerClient {
public:
    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const OVERRIDE { return true; }
    virtual void ignoreWordInSpellDocument(const String&) OVERRIDE { }
    virtual void learnWord(const String&) OVERRIDE { }
    virtual void checkSpellingOfString(const UChar*, int, int*, int*) OVERRIDE { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) OVERRIDE { return String(); }
    virtual void checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*) OVERRIDE { }

#if USE(UNIFIED_TEXT_CHECKING)
    virtual void checkTextOfParagraph(const UChar*, int, TextCheckingTypeMask, Vector<TextCheckingResult>&) OVERRIDE { };
#endif

    virtual void getGuessesForWord(const String&, const String&, Vector<String>&) OVERRIDE { }
    virtual void requestCheckingOfString(PassRefPtr<TextCheckingRequest>) OVERRIDE;
};

class EmptyEditorClient : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }
    virtual void pageDestroyed() OVERRIDE { }

    virtual bool shouldDeleteRange(Range*) OVERRIDE { return false; }
    virtual bool smartInsertDeleteEnabled() OVERRIDE { return false; }
    virtual bool isSelectTrailingWhitespaceEnabled() OVERRIDE { return false; }
    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE { return false; }
    virtual void toggleContinuousSpellChecking() OVERRIDE { }
    virtual bool isGrammarCheckingEnabled() OVERRIDE { return false; }
    virtual void toggleGrammarChecking() OVERRIDE { }
    virtual int spellCheckerDocumentTag() OVERRIDE { return -1; }


    virtual bool shouldBeginEditing(Range*) OVERRIDE { return false; }
    virtual bool shouldEndEditing(Range*) OVERRIDE { return false; }
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) OVERRIDE { return false; }
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) OVERRIDE { return false; }
    virtual bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) OVERRIDE { return false; }

    virtual bool shouldApplyStyle(StylePropertySet*, Range*) OVERRIDE { return false; }
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*) OVERRIDE { return false; }

    virtual void didBeginEditing() OVERRIDE { }
    virtual void respondToChangedContents() OVERRIDE { }
    virtual void respondToChangedSelection(Frame*) OVERRIDE { }
    virtual void didEndEditing() OVERRIDE { }
    virtual void willWriteSelectionToPasteboard(Range*) OVERRIDE { }
    virtual void didWriteSelectionToPasteboard() OVERRIDE { }
    virtual void getClientPasteboardDataForRange(Range*, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) OVERRIDE { }

    virtual void registerUndoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void registerRedoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void clearUndoRedoOperations() OVERRIDE { }

    virtual bool canCopyCut(Frame*, bool defaultValue) const OVERRIDE { return defaultValue; }
    virtual bool canPaste(Frame*, bool defaultValue) const OVERRIDE { return defaultValue; }
    virtual bool canUndo() const OVERRIDE { return false; }
    virtual bool canRedo() const OVERRIDE { return false; }

    virtual void undo() OVERRIDE { }
    virtual void redo() OVERRIDE { }

    virtual void handleKeyboardEvent(KeyboardEvent*) OVERRIDE { }
    virtual void handleInputMethodKeydown(KeyboardEvent*) OVERRIDE { }

    virtual void textFieldDidBeginEditing(Element*) OVERRIDE { }
    virtual void textFieldDidEndEditing(Element*) OVERRIDE { }
    virtual void textDidChangeInTextField(Element*) OVERRIDE { }
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) OVERRIDE { return false; }
    virtual void textWillBeDeletedInTextField(Element*) OVERRIDE { }
    virtual void textDidChangeInTextArea(Element*) OVERRIDE { }

#if PLATFORM(MAC)
    virtual NSString* userVisibleString(NSURL*) OVERRIDE { return 0; }
    virtual DocumentFragment* documentFragmentFromAttributedString(NSAttributedString*, Vector<RefPtr<ArchiveResource>>&) OVERRIDE { return 0; };
    virtual void setInsertionPasteboard(const String&) OVERRIDE { };
    virtual NSURL *canonicalizeURL(NSURL*) OVERRIDE { return 0; }
    virtual NSURL *canonicalizeURLString(NSString*) OVERRIDE { return 0; }
#endif

#if USE(APPKIT)
    virtual void uppercaseWord() OVERRIDE { }
    virtual void lowercaseWord() OVERRIDE { }
    virtual void capitalizeWord() OVERRIDE { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool) OVERRIDE { }
    virtual bool substitutionsPanelIsShowing() OVERRIDE { return false; }
    virtual void toggleSmartInsertDelete() OVERRIDE { }
    virtual bool isAutomaticQuoteSubstitutionEnabled() OVERRIDE { return false; }
    virtual void toggleAutomaticQuoteSubstitution() OVERRIDE { }
    virtual bool isAutomaticLinkDetectionEnabled() OVERRIDE { return false; }
    virtual void toggleAutomaticLinkDetection() OVERRIDE { }
    virtual bool isAutomaticDashSubstitutionEnabled() OVERRIDE { return false; }
    virtual void toggleAutomaticDashSubstitution() OVERRIDE { }
    virtual bool isAutomaticTextReplacementEnabled() OVERRIDE { return false; }
    virtual void toggleAutomaticTextReplacement() OVERRIDE { }
    virtual bool isAutomaticSpellingCorrectionEnabled() OVERRIDE { return false; }
    virtual void toggleAutomaticSpellingCorrection() OVERRIDE { }
#endif

#if ENABLE(DELETION_UI)
    virtual bool shouldShowDeleteInterface(HTMLElement*) OVERRIDE { return false; }
#endif

#if PLATFORM(GTK)
    virtual bool shouldShowUnicodeMenu() OVERRIDE { return false; }
#endif
    virtual TextCheckerClient* textChecker() OVERRIDE { return &m_textCheckerClient; }

    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) OVERRIDE { }
    virtual void updateSpellingUIWithMisspelledWord(const String&) OVERRIDE { }
    virtual void showSpellingUI(bool) OVERRIDE { }
    virtual bool spellingUIIsShowing() OVERRIDE { return false; }

    virtual void willSetInputMethodState() OVERRIDE { }
    virtual void setInputMethodState(bool) OVERRIDE { }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

#if ENABLE(CONTEXT_MENUS)
class EmptyContextMenuClient : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void contextMenuDestroyed() OVERRIDE { }

#if USE(CROSS_PLATFORM_CONTEXT_MENUS)
    virtual PassOwnPtr<ContextMenu> customizeMenu(PassOwnPtr<ContextMenu>) OVERRIDE;
#else
    virtual PlatformMenuDescription getCustomMenuFromDefaultItems(ContextMenu*) OVERRIDE { return 0; }
#endif
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*) OVERRIDE { }

    virtual void downloadURL(const URL&) OVERRIDE { }
    virtual void searchWithGoogle(const Frame*) OVERRIDE { }
    virtual void lookUpInDictionary(Frame*) OVERRIDE { }
    virtual bool isSpeaking() OVERRIDE { return false; }
    virtual void speak(const String&) OVERRIDE { }
    virtual void stopSpeaking() OVERRIDE { }

#if PLATFORM(MAC)
    virtual void searchWithSpotlight() OVERRIDE { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    virtual void showContextMenu() OVERRIDE { }
#endif
};
#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(DRAG_SUPPORT)
class EmptyDragClient : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual void willPerformDragDestinationAction(DragDestinationAction, DragData&) OVERRIDE { }
    virtual void willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard&) OVERRIDE { }
    virtual DragDestinationAction actionMaskForDrag(DragData&) OVERRIDE { return DragDestinationActionNone; }
    virtual DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) OVERRIDE { return DragSourceActionNone; }
    virtual void startDrag(DragImageRef, const IntPoint&, const IntPoint&, Clipboard&, Frame&, bool) OVERRIDE { }
    virtual void dragControllerDestroyed() OVERRIDE { }
};
#endif // ENABLE(DRAG_SUPPORT)

class EmptyInspectorClient : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    virtual void inspectorDestroyed() OVERRIDE { }
    
    virtual InspectorFrontendChannel* openInspectorFrontend(InspectorController*) OVERRIDE { return 0; }
    virtual void closeInspectorFrontend() OVERRIDE { }
    virtual void bringFrontendToFront() OVERRIDE { }

    virtual void highlight() OVERRIDE { }
    virtual void hideHighlight() OVERRIDE { }
};

class EmptyDeviceClient : public DeviceClient {
public:
    virtual void startUpdating() OVERRIDE { }
    virtual void stopUpdating() OVERRIDE { }
};

class EmptyDeviceMotionClient : public DeviceMotionClient {
public:
    virtual void setController(DeviceMotionController*) OVERRIDE { }
    virtual DeviceMotionData* lastMotion() const OVERRIDE { return 0; }
    virtual void deviceMotionControllerDestroyed() OVERRIDE { }
};

class EmptyDeviceOrientationClient : public DeviceOrientationClient {
public:
    virtual void setController(DeviceOrientationController*) OVERRIDE { }
    virtual DeviceOrientationData* lastOrientation() const OVERRIDE { return 0; }
    virtual void deviceOrientationControllerDestroyed() OVERRIDE { }
};

void fillWithEmptyClients(Page::PageClients&);

}

#endif // EmptyClients_h
