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
#include "ProgressTrackerClient.h"
#include "ResourceError.h"
#include <wtf/text/StringView.h>

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
    virtual void chromeDestroyed() override { }

    virtual void setWindowRect(const FloatRect&) override { }
    virtual FloatRect windowRect() override { return FloatRect(); }

    virtual FloatRect pageRect() override { return FloatRect(); }

    virtual void focus() override { }
    virtual void unfocus() override { }

    virtual bool canTakeFocus(FocusDirection) override { return false; }
    virtual void takeFocus(FocusDirection) override { }

    virtual void focusedElementChanged(Element*) override { }
    virtual void focusedFrameChanged(Frame*) override { }

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) override { return 0; }
    virtual void show() override { }

    virtual bool canRunModal() override { return false; }
    virtual void runModal() override { }

    virtual void setToolbarsVisible(bool) override { }
    virtual bool toolbarsVisible() override { return false; }

    virtual void setStatusbarVisible(bool) override { }
    virtual bool statusbarVisible() override { return false; }

    virtual void setScrollbarsVisible(bool) override { }
    virtual bool scrollbarsVisible() override { return false; }

    virtual void setMenubarVisible(bool) override { }
    virtual bool menubarVisible() override { return false; }

    virtual void setResizable(bool) override { }

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, unsigned, const String&) override { }

    virtual bool canRunBeforeUnloadConfirmPanel() override { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, Frame*) override { return true; }

    virtual void closeWindowSoon() override { }

    virtual void runJavaScriptAlert(Frame*, const String&) override { }
    virtual bool runJavaScriptConfirm(Frame*, const String&) override { return false; }
    virtual bool runJavaScriptPrompt(Frame*, const String&, const String&, String&) override { return false; }
    virtual bool shouldInterruptJavaScript() override { return false; }

    virtual bool selectItemWritingDirectionIsNatural() override { return false; }
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override { return false; }
    virtual bool hasOpenedPopup() const override { return false; }
    virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const override;
    virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const override;

    virtual void setStatusbarText(const String&) override { }

    virtual KeyboardUIMode keyboardUIMode() override { return KeyboardAccessDefault; }

    virtual IntRect windowResizerRect() const override { return IntRect(); }

    virtual void invalidateRootView(const IntRect&, bool) override { }
    virtual void invalidateContentsAndRootView(const IntRect&, bool) override { }
    virtual void invalidateContentsForSlowScroll(const IntRect&, bool) override { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) override { }
#if USE(TILED_BACKING_STORE)
    virtual void delegatedScrollRequested(const IntPoint&) { }
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    virtual void scheduleAnimation() { }
#endif

    virtual IntPoint screenToRootView(const IntPoint& p) const override { return p; }
    virtual IntRect rootViewToScreen(const IntRect& r) const override { return r; }
    virtual PlatformPageClient platformPageClient() const override { return 0; }
    virtual void contentsSizeChanged(Frame*, const IntSize&) const override { }

    virtual void scrollbarsModeDidChange() const override { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned) override { }

    virtual void setToolTip(const String&, TextDirection) override { }

    virtual void print(Frame*) override { }

#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(Frame*, const String&, DatabaseDetails) override { }
#endif

    virtual void reachedMaxAppCacheSize(int64_t) override { }
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t) override { }

#if ENABLE(DIRECTORY_UPLOAD)
    virtual void enumerateChosenDirectory(FileChooser*) { }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) override;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES) && !PLATFORM(IOS)
    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) override;
#endif

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) override;
    virtual void loadIconForFiles(const Vector<String>&, FileIconLoader*) override { }

    virtual void formStateDidChange(const Node*) override { }

    virtual void elementDidFocus(const Node*) override { }
    virtual void elementDidBlur(const Node*) override { }

#if !PLATFORM(IOS)
    virtual void setCursor(const Cursor&) override { }
    virtual void setCursorHiddenUntilMouseMoves(bool) override { }
#endif

    virtual void scrollRectIntoView(const IntRect&) const override { }

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) override { }
    virtual void setNeedsOneShotDrawingSynchronization() override { }
    virtual void scheduleCompositingLayerFlush() override { }
#endif

#if PLATFORM(WIN)
    virtual void setLastSetCursorToCurrentCursor() override { }
    virtual void AXStartFrameLoad() override { }
    virtual void AXFinishFrameLoad() override { }
#endif

#if PLATFORM(IOS)
#if ENABLE(TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() override { }
#endif
    virtual void didReceiveMobileDocType() override { }
    virtual void setNeedsScrollNotifications(Frame*, bool) override { }
    virtual void observedContentChange(Frame*) override { }
    virtual void clearContentChangeObservers(Frame*) override { }
    virtual void notifyRevealedSelectionByScrollingFrame(Frame*) override { }
    virtual void didLayout(LayoutType) override { }
    virtual void didStartOverflowScroll() override { }
    virtual void didEndOverflowScroll() override { }

    virtual void suppressFormNotifications() override { }
    virtual void restoreFormNotifications() override { }

    virtual void addOrUpdateScrollingLayer(Node*, PlatformLayer*, PlatformLayer*, const IntSize&, bool, bool) override { }
    virtual void removeScrollingLayer(Node*, PlatformLayer*, PlatformLayer*) override { }

    virtual void webAppOrientationsUpdated() override { };
#endif // PLATFORM(IOS)

#if PLATFORM(IOS)
    virtual bool isStopping() override { return false; }
#endif

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) override { }
#endif
    
    virtual void numWheelEventHandlersChanged(unsigned) override { }
    
    virtual bool isEmptyChromeClient() const override { return true; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element>>&) override { }
    virtual bool shouldNotifyOnFormChanges() override { return false; }
};

// FIXME (bug 116233): Get rid of EmptyFrameLoaderClient. It is a travesty.

class EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() override { }

    virtual bool hasWebView() const override { return true; } // mainly for assertions

    virtual void makeRepresentation(DocumentLoader*) override { }
    virtual void forceLayout() override { }
#if PLATFORM(IOS)
    virtual void forceLayoutWithoutRecalculatingStyles() override { }
#endif
    virtual void forceLayoutForNonHTML() override { }

    virtual void setCopiesOnScroll() override { }

    virtual void detachedFromParent2() override { }
    virtual void detachedFromParent3() override { }

    virtual void convertMainResourceLoadToDownload(DocumentLoader*, const ResourceRequest&, const ResourceResponse&) override { }

    virtual void assignIdentifierToInitialRequest(unsigned long, DocumentLoader*, const ResourceRequest&) override { }
    virtual bool shouldUseCredentialStorage(DocumentLoader*, unsigned long) override { return false; }
    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) override { }
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) override { }
    virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long, const AuthenticationChallenge&) override { }
#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
    virtual bool canAuthenticateAgainstProtectionSpace(DocumentLoader*, unsigned long, const ProtectionSpace&) override { return false; }
#endif

#if PLATFORM(IOS)
    virtual RetainPtr<CFDictionaryRef> connectionProperties(DocumentLoader*, unsigned long) override { return nullptr; }
#endif

    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) override { }
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long, int) override { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) override { }
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&) override { }
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) override { return false; }

    virtual void dispatchDidHandleOnloadEvents() override { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() override { }
    virtual void dispatchDidCancelClientRedirect() override { }
    virtual void dispatchWillPerformClientRedirect(const URL&, double, double) override { }
    virtual void dispatchDidChangeLocationWithinPage() override { }
    virtual void dispatchDidPushStateWithinPage() override { }
    virtual void dispatchDidReplaceStateWithinPage() override { }
    virtual void dispatchDidPopStateWithinPage() override { }
    virtual void dispatchWillClose() override { }
    virtual void dispatchDidReceiveIcon() override { }
    virtual void dispatchDidStartProvisionalLoad() override { }
    virtual void dispatchDidReceiveTitle(const StringWithDirection&) override { }
    virtual void dispatchDidChangeIcons(IconType) override { }
    virtual void dispatchDidCommitLoad() override { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) override { }
    virtual void dispatchDidFailLoad(const ResourceError&) override { }
    virtual void dispatchDidFinishDocumentLoad() override { }
    virtual void dispatchDidFinishLoad() override { }
    virtual void dispatchDidLayout(LayoutMilestones) override { }

    virtual Frame* dispatchCreatePage(const NavigationAction&) override { return 0; }
    virtual void dispatchShow() override { }

    virtual void dispatchDecidePolicyForResponse(const ResourceResponse&, const ResourceRequest&, FramePolicyFunction) override { }
    virtual void dispatchDecidePolicyForNewWindowAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, const String&, FramePolicyFunction) override;
    virtual void dispatchDecidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, FramePolicyFunction) override;
    virtual void cancelPolicyCheck() override { }

    virtual void dispatchUnableToImplementPolicy(const ResourceError&) override { }

    virtual void dispatchWillSendSubmitEvent(PassRefPtr<FormState>) override;
    virtual void dispatchWillSubmitForm(PassRefPtr<FormState>, FramePolicyFunction) override;

    virtual void revertToProvisionalState(DocumentLoader*) override { }
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) override { }

    virtual void setMainFrameDocumentReady(bool) override { }

    virtual void startDownload(const ResourceRequest&, const String& suggestedName = String()) override { UNUSED_PARAM(suggestedName); }

    virtual void willChangeTitle(DocumentLoader*) override { }
    virtual void didChangeTitle(DocumentLoader*) override { }

    virtual void committedLoad(DocumentLoader*, const char*, int) override { }
    virtual void finishedLoading(DocumentLoader*) override { }

    virtual ResourceError cancelledError(const ResourceRequest&) override { ResourceError error("", 0, "", ""); error.setIsCancellation(true); return error; }
    virtual ResourceError blockedError(const ResourceRequest&) override { return ResourceError("", 0, "", ""); }
    virtual ResourceError cannotShowURLError(const ResourceRequest&) override { return ResourceError("", 0, "", ""); }
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) override { return ResourceError("", 0, "", ""); }

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) override { return ResourceError("", 0, "", ""); }
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) override { return ResourceError("", 0, "", ""); }
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) override { return ResourceError("", 0, "", ""); }

    virtual bool shouldFallBack(const ResourceError&) override { return false; }

    virtual bool canHandleRequest(const ResourceRequest&) const override { return false; }
    virtual bool canShowMIMEType(const String&) const override { return false; }
    virtual bool canShowMIMETypeAsHTML(const String&) const override { return false; }
    virtual bool representationExistsForURLScheme(const String&) const override { return false; }
    virtual String generatedMIMETypeForURLScheme(const String&) const override { return ""; }

    virtual void frameLoadCompleted() override { }
    virtual void restoreViewState() override { }
    virtual void provisionalLoadStarted() override { }
    virtual void didFinishLoad() override { }
    virtual void prepareForDataSourceReplacement() override { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) override;
    virtual void setTitle(const StringWithDirection&, const URL&) override { }

    virtual String userAgent(const URL&) override { return ""; }

    virtual void savePlatformDataToCachedFrame(CachedFrame*) override { }
    virtual void transitionToCommittedFromCachedFrame(CachedFrame*) override { }
#if PLATFORM(IOS)
    virtual void didRestoreFrameHierarchyForCachedFrame() override { }
#endif
    virtual void transitionToCommittedForNewPage() override { }

    virtual void didSaveToPageCache() override { }
    virtual void didRestoreFromPageCache() override { }

    virtual void dispatchDidBecomeFrameset(bool) override { }

    virtual void updateGlobalHistory() override { }
    virtual void updateGlobalHistoryRedirectLinks() override { }
    virtual bool shouldGoToHistoryItem(HistoryItem*) const override { return false; }
    virtual bool shouldStopLoadingForHistoryItem(HistoryItem*) const override { return false; }
    virtual void updateGlobalHistoryItemForPage() override { }
    virtual void saveViewStateToItem(HistoryItem*) override { }
    virtual bool canCachePage() const override { return false; }
    virtual void didDisplayInsecureContent() override { }
    virtual void didRunInsecureContent(SecurityOrigin*, const URL&) override { }
    virtual void didDetectXSS(const URL&, bool) override { }
    virtual PassRefPtr<Frame> createFrame(const URL&, const String&, HTMLFrameOwnerElement*, const String&, bool, int, int) override;
    virtual PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&, bool) override;
    virtual void recreatePlugin(Widget*) override;
    virtual PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const URL&, const Vector<String>&, const Vector<String>&) override;
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual PassRefPtr<Widget> createMediaPlayerProxyPlugin(const IntSize&, HTMLMediaElement*, const URL&, const Vector<String>&, const Vector<String>&, const String&) override;
    virtual void hideMediaPlayerProxyPlugin(Widget*) override { }
    virtual void showMediaPlayerProxyPlugin(Widget*) override { }
#endif

    virtual ObjectContentType objectContentType(const URL&, const String&, bool) override { return ObjectContentType(); }
    virtual String overrideMediaType() const override { return String(); }

    virtual void redirectDataToPlugin(Widget*) override { }
    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) override { }

    virtual void registerForIconNotification(bool) override { }

#if PLATFORM(MAC)
    virtual RemoteAXObjectRef accessibilityRemoteObject() override { return 0; }
    virtual NSCachedURLResponse* willCacheResponse(DocumentLoader*, unsigned long, NSCachedURLResponse* response) const override { return response; }
#endif
#if PLATFORM(WIN) && USE(CFNETWORK)
    // FIXME: Windows should use willCacheResponse - <https://bugs.webkit.org/show_bug.cgi?id=57257>.
    virtual bool shouldCacheResponse(DocumentLoader*, unsigned long, const ResourceResponse&, const unsigned char*, unsigned long long) override { return true; }
#endif

    virtual PassRefPtr<FrameNetworkingContext> createNetworkingContext() override;

    virtual bool isEmptyFrameLoaderClient() override { return true; }
};

class EmptyTextCheckerClient : public TextCheckerClient {
public:
    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const override { return true; }
    virtual void ignoreWordInSpellDocument(const String&) override { }
    virtual void learnWord(const String&) override { }
    virtual void checkSpellingOfString(const UChar*, int, int*, int*) override { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) override { return String(); }
    virtual void checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*) override { }

#if USE(UNIFIED_TEXT_CHECKING)
    virtual Vector<TextCheckingResult> checkTextOfParagraph(StringView, TextCheckingTypeMask) override { return Vector<TextCheckingResult>(); }
#endif

    virtual void getGuessesForWord(const String&, const String&, Vector<String>&) override { }
    virtual void requestCheckingOfString(PassRefPtr<TextCheckingRequest>) override;
};

class EmptyEditorClient : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }
    virtual void pageDestroyed() override { }

    virtual bool shouldDeleteRange(Range*) override { return false; }
    virtual bool smartInsertDeleteEnabled() override { return false; }
    virtual bool isSelectTrailingWhitespaceEnabled() override { return false; }
    virtual bool isContinuousSpellCheckingEnabled() override { return false; }
    virtual void toggleContinuousSpellChecking() override { }
    virtual bool isGrammarCheckingEnabled() override { return false; }
    virtual void toggleGrammarChecking() override { }
    virtual int spellCheckerDocumentTag() override { return -1; }


    virtual bool shouldBeginEditing(Range*) override { return false; }
    virtual bool shouldEndEditing(Range*) override { return false; }
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) override { return false; }
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) override { return false; }
    virtual bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) override { return false; }

    virtual bool shouldApplyStyle(StyleProperties*, Range*) override { return false; }
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*) override { return false; }

    virtual void didBeginEditing() override { }
    virtual void respondToChangedContents() override { }
    virtual void respondToChangedSelection(Frame*) override { }
    virtual void didEndEditing() override { }
    virtual void willWriteSelectionToPasteboard(Range*) override { }
    virtual void didWriteSelectionToPasteboard() override { }
    virtual void getClientPasteboardDataForRange(Range*, Vector<String>&, Vector<RefPtr<SharedBuffer>>&) override { }

    virtual void registerUndoStep(PassRefPtr<UndoStep>) override;
    virtual void registerRedoStep(PassRefPtr<UndoStep>) override;
    virtual void clearUndoRedoOperations() override { }

    virtual bool canCopyCut(Frame*, bool defaultValue) const override { return defaultValue; }
    virtual bool canPaste(Frame*, bool defaultValue) const override { return defaultValue; }
    virtual bool canUndo() const override { return false; }
    virtual bool canRedo() const override { return false; }

    virtual void undo() override { }
    virtual void redo() override { }

    virtual void handleKeyboardEvent(KeyboardEvent*) override { }
    virtual void handleInputMethodKeydown(KeyboardEvent*) override { }

    virtual void textFieldDidBeginEditing(Element*) override { }
    virtual void textFieldDidEndEditing(Element*) override { }
    virtual void textDidChangeInTextField(Element*) override { }
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) override { return false; }
    virtual void textWillBeDeletedInTextField(Element*) override { }
    virtual void textDidChangeInTextArea(Element*) override { }

#if PLATFORM(IOS)
    virtual void suppressSelectionNotifications() override { }
    virtual void restoreSelectionNotifications() override { }
    virtual void startDelayingAndCoalescingContentChangeNotifications() override { }
    virtual void stopDelayingAndCoalescingContentChangeNotifications() override { }
    virtual void writeDataToPasteboard(NSDictionary*) override { }
    virtual NSArray* supportedPasteboardTypesForCurrentSelection() override { return nullptr; }
    virtual NSArray* readDataFromPasteboard(NSString*, int) override { return nullptr; }
    virtual bool hasRichlyEditableSelection() override { return false; }
    virtual int getPasteboardItemsCount() override { return 0; }
    virtual DocumentFragment* documentFragmentFromDelegate(int) override { return nullptr; }
    virtual bool performsTwoStepPaste(DocumentFragment*) override { return false; }
    virtual int pasteboardChangeCount() override { return 0; }
#endif

#if PLATFORM(MAC)
    virtual NSString* userVisibleString(NSURL*) override { return 0; }
    virtual DocumentFragment* documentFragmentFromAttributedString(NSAttributedString*, Vector<RefPtr<ArchiveResource>>&) override { return 0; };
    virtual void setInsertionPasteboard(const String&) override { };
    virtual NSURL *canonicalizeURL(NSURL*) override { return 0; }
    virtual NSURL *canonicalizeURLString(NSString*) override { return 0; }
#endif

#if USE(APPKIT)
    virtual void uppercaseWord() override { }
    virtual void lowercaseWord() override { }
    virtual void capitalizeWord() override { }
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool) override { }
    virtual bool substitutionsPanelIsShowing() override { return false; }
    virtual void toggleSmartInsertDelete() override { }
    virtual bool isAutomaticQuoteSubstitutionEnabled() override { return false; }
    virtual void toggleAutomaticQuoteSubstitution() override { }
    virtual bool isAutomaticLinkDetectionEnabled() override { return false; }
    virtual void toggleAutomaticLinkDetection() override { }
    virtual bool isAutomaticDashSubstitutionEnabled() override { return false; }
    virtual void toggleAutomaticDashSubstitution() override { }
    virtual bool isAutomaticTextReplacementEnabled() override { return false; }
    virtual void toggleAutomaticTextReplacement() override { }
    virtual bool isAutomaticSpellingCorrectionEnabled() override { return false; }
    virtual void toggleAutomaticSpellingCorrection() override { }
#endif

#if ENABLE(DELETION_UI)
    virtual bool shouldShowDeleteInterface(HTMLElement*) override { return false; }
#endif

#if PLATFORM(GTK)
    virtual bool shouldShowUnicodeMenu() override { return false; }
#endif
    virtual TextCheckerClient* textChecker() override { return &m_textCheckerClient; }

    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) override { }
    virtual void updateSpellingUIWithMisspelledWord(const String&) override { }
    virtual void showSpellingUI(bool) override { }
    virtual bool spellingUIIsShowing() override { return false; }

    virtual void willSetInputMethodState() override { }
    virtual void setInputMethodState(bool) override { }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

#if ENABLE(CONTEXT_MENUS)
class EmptyContextMenuClient : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void contextMenuDestroyed() override { }

#if USE(CROSS_PLATFORM_CONTEXT_MENUS)
    virtual PassOwnPtr<ContextMenu> customizeMenu(PassOwnPtr<ContextMenu>) override;
#else
    virtual PlatformMenuDescription getCustomMenuFromDefaultItems(ContextMenu*) override { return 0; }
#endif
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*) override { }

    virtual void downloadURL(const URL&) override { }
    virtual void searchWithGoogle(const Frame*) override { }
    virtual void lookUpInDictionary(Frame*) override { }
    virtual bool isSpeaking() override { return false; }
    virtual void speak(const String&) override { }
    virtual void stopSpeaking() override { }

#if PLATFORM(MAC)
    virtual void searchWithSpotlight() override { }
#endif

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    virtual void showContextMenu() override { }
#endif
};
#endif // ENABLE(CONTEXT_MENUS)

#if ENABLE(DRAG_SUPPORT)
class EmptyDragClient : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual void willPerformDragDestinationAction(DragDestinationAction, DragData&) override { }
    virtual void willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard&) override { }
    virtual DragDestinationAction actionMaskForDrag(DragData&) override { return DragDestinationActionNone; }
    virtual DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) override { return DragSourceActionNone; }
    virtual void startDrag(DragImageRef, const IntPoint&, const IntPoint&, Clipboard&, Frame&, bool) override { }
    virtual void dragControllerDestroyed() override { }
};
#endif // ENABLE(DRAG_SUPPORT)

class EmptyInspectorClient : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    virtual void inspectorDestroyed() override { }
    
    virtual InspectorFrontendChannel* openInspectorFrontend(InspectorController*) override { return 0; }
    virtual void closeInspectorFrontend() override { }
    virtual void bringFrontendToFront() override { }

    virtual void highlight() override { }
    virtual void hideHighlight() override { }
};

class EmptyDeviceClient : public DeviceClient {
public:
    virtual void startUpdating() override { }
    virtual void stopUpdating() override { }
};

class EmptyDeviceMotionClient : public DeviceMotionClient {
public:
    virtual void setController(DeviceMotionController*) override { }
    virtual DeviceMotionData* lastMotion() const override { return 0; }
    virtual void deviceMotionControllerDestroyed() override { }
};

class EmptyDeviceOrientationClient : public DeviceOrientationClient {
public:
    virtual void setController(DeviceOrientationController*) override { }
    virtual DeviceOrientationData* lastOrientation() const override { return 0; }
    virtual void deviceOrientationControllerDestroyed() override { }
};

class EmptyProgressTrackerClient : public ProgressTrackerClient {
    virtual void willChangeEstimatedProgress() override { }
    virtual void didChangeEstimatedProgress() override { }

    virtual void progressStarted(Frame&) override { }
    virtual void progressEstimateChanged(Frame&) override { }
    virtual void progressFinished(Frame&) override { }
};

void fillWithEmptyClients(Page::PageClients&);

}

#endif // EmptyClients_h
