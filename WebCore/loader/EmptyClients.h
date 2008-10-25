/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
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
#include "DragClient.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "EditorClient.h"
#include "FocusDirection.h"
#include "FloatRect.h"
#include "FrameLoaderClient.h"
#include "InspectorClient.h"
#include "ResourceError.h"
#include "SharedBuffer.h"

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

class EmptyChromeClient : public ChromeClient {
public:
    virtual ~EmptyChromeClient() { }
    virtual void chromeDestroyed() { }

    virtual void setWindowRect(const FloatRect&) { }
    virtual FloatRect windowRect() { return FloatRect(); }

    virtual FloatRect pageRect() { return FloatRect(); }

    virtual float scaleFactor() { return 1.f; }

    virtual void focus() { }
    virtual void unfocus() { }

    virtual bool canTakeFocus(FocusDirection) { return false; }
    virtual void takeFocus(FocusDirection) { }

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&) { return 0; }
    virtual void show() { }

    virtual bool canRunModal() { return false; }
    virtual void runModal() { }

    virtual void setToolbarsVisible(bool) { }
    virtual bool toolbarsVisible() { return false; }

    virtual void setStatusbarVisible(bool) { }
    virtual bool statusbarVisible() { return false; }

    virtual void setScrollbarsVisible(bool) { }
    virtual bool scrollbarsVisible() { return false; }

    virtual void setMenubarVisible(bool) { }
    virtual bool menubarVisible() { return false; }

    virtual void setResizable(bool) { }

    virtual void addMessageToConsole(const String& message, unsigned int lineNumber, const String& sourceID) { }

    virtual bool canRunBeforeUnloadConfirmPanel() { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame* frame) { return true; }

    virtual void closeWindowSoon() { }

    virtual void runJavaScriptAlert(Frame*, const String&) { }
    virtual bool runJavaScriptConfirm(Frame*, const String&) { return false; }
    virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) { return false; }
    virtual bool shouldInterruptJavaScript() { return false; }

    virtual void setStatusbarText(const String&) { }

    virtual bool tabsToLinks() const { return false; }

    virtual IntRect windowResizerRect() const { return IntRect(); }
    virtual void addToDirtyRegion(const IntRect&) { }
    virtual void scrollBackingStore(int dx, int dy, const IntRect& scrollViewRect, const IntRect& clipRect) { }
    virtual void updateBackingStore() { }

    virtual void repaint(const IntRect&, bool contentChanged, bool immediate = false, bool repaintContentOnly = false) { }
    virtual void scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) { }
    virtual IntPoint screenToWindow(const IntPoint& p) const { return p; }
    virtual IntRect windowToScreen(const IntRect& r) const { return r; }
    virtual PlatformWidget platformWindow() const { return 0; }

    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) { }

    virtual void setToolTip(const String&) { }

    virtual void print(Frame*) { }

    virtual void exceededDatabaseQuota(Frame*, const String&) { }

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) { }
};

class EmptyFrameLoaderClient : public FrameLoaderClient {
public:
    virtual ~EmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() { }

    virtual bool hasWebView() const { return true; } // mainly for assertions

    virtual void makeRepresentation(DocumentLoader*) { }
    virtual void forceLayout() { }
    virtual void forceLayoutForNonHTML() { }

    virtual void updateHistoryForCommit() { }

    virtual void updateHistoryForBackForwardNavigation() { }
    virtual void updateHistoryForReload() { }
    virtual void updateHistoryForStandardLoad() { }
    virtual void updateHistoryForInternalLoad() { }

    virtual void updateHistoryAfterClientRedirect() { }

    virtual void setCopiesOnScroll() { }

    virtual void detachedFromParent2() { }
    virtual void detachedFromParent3() { }

    virtual void download(ResourceHandle*, const ResourceRequest&, const ResourceRequest&, const ResourceResponse&) { }

    virtual void assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&) { }
    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse) { }
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) { }
    virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long identifier, const ResourceResponse&) { }
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, unsigned long identifier, int lengthReceived) { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long identifier) { }
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long identifier, const ResourceError&) { }
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int length) { return false; }

    virtual void dispatchDidHandleOnloadEvents() { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() { }
    virtual void dispatchDidCancelClientRedirect() { }
    virtual void dispatchWillPerformClientRedirect(const KURL&, double interval, double fireDate) { }
    virtual void dispatchDidChangeLocationWithinPage() { }
    virtual void dispatchWillClose() { }
    virtual void dispatchDidReceiveIcon() { }
    virtual void dispatchDidStartProvisionalLoad() { }
    virtual void dispatchDidReceiveTitle(const String& title) { }
    virtual void dispatchDidCommitLoad() { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) { }
    virtual void dispatchDidFailLoad(const ResourceError&) { }
    virtual void dispatchDidFinishDocumentLoad() { }
    virtual void dispatchDidFinishLoad() { }
    virtual void dispatchDidFirstLayout() { }

    virtual Frame* dispatchCreatePage() { return 0; }
    virtual void dispatchShow() { }

    virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, const ResourceRequest&) { }
    virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>, const String& frameName) { }
    virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, PassRefPtr<FormState>) { }
    virtual void cancelPolicyCheck() { }

    virtual void dispatchUnableToImplementPolicy(const ResourceError&) { }

    virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>) { }

    virtual void dispatchDidLoadMainResource(DocumentLoader*) { }
    virtual void revertToProvisionalState(DocumentLoader*) { }
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) { }

    virtual void willChangeEstimatedProgress() { }
    virtual void didChangeEstimatedProgress() { }
    virtual void postProgressStartedNotification() { }
    virtual void postProgressEstimateChangedNotification() { }
    virtual void postProgressFinishedNotification() { }

    virtual void setMainFrameDocumentReady(bool) { }

    virtual void startDownload(const ResourceRequest&) { }

    virtual void willChangeTitle(DocumentLoader*) { }
    virtual void didChangeTitle(DocumentLoader*) { }

    virtual void committedLoad(DocumentLoader*, const char*, int) { }
    virtual void finishedLoading(DocumentLoader*) { }

    virtual ResourceError cancelledError(const ResourceRequest&) { return ResourceError(); }
    virtual ResourceError blockedError(const ResourceRequest&) { return ResourceError(); }
    virtual ResourceError cannotShowURLError(const ResourceRequest&) { return ResourceError(); }
    virtual ResourceError interruptForPolicyChangeError(const ResourceRequest&) { return ResourceError(); }

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) { return ResourceError(); }
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) { return ResourceError(); }
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) { return ResourceError(); }

    virtual bool shouldFallBack(const ResourceError&) { return false; }

    virtual bool canHandleRequest(const ResourceRequest&) const { return false; }
    virtual bool canShowMIMEType(const String& MIMEType) const { return false; }
    virtual bool representationExistsForURLScheme(const String& URLScheme) const { return false; }
    virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const { return ""; }

    virtual void frameLoadCompleted() { }
    virtual void restoreViewState() { }
    virtual void provisionalLoadStarted() { }
    virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const { return false; }
    virtual void addHistoryItemForFragmentScroll() { }
    virtual void didFinishLoad() { }
    virtual void prepareForDataSourceReplacement() { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData) { return DocumentLoader::create(request, substituteData); }
    virtual void setTitle(const String& title, const KURL&) { }

    virtual String userAgent(const KURL&) { return ""; }

    virtual void savePlatformDataToCachedPage(CachedPage*) { }
    virtual void transitionToCommittedFromCachedPage(CachedPage*) { }
    virtual void transitionToCommittedForNewPage() { }    

    virtual void updateGlobalHistory(const KURL&) { }
    virtual bool shouldGoToHistoryItem(HistoryItem*) const { return false; }
    virtual void saveViewStateToItem(HistoryItem*) { }
    virtual bool canCachePage() const { return false; }

    virtual PassRefPtr<Frame> createFrame(const KURL& url, const String& name, HTMLFrameOwnerElement* ownerElement,
                               const String& referrer, bool allowsScrolling, int marginWidth, int marginHeight) { return 0; }
    virtual Widget* createPlugin(const IntSize&,Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool) { return 0; }
    virtual Widget* createJavaAppletWidget(const IntSize&, Element*, const KURL&, const Vector<String>&, const Vector<String>&) { return 0; }

    virtual ObjectContentType objectContentType(const KURL& url, const String& mimeType) { return ObjectContentType(); }
    virtual String overrideMediaType() const { return String(); }

    virtual void redirectDataToPlugin(Widget*) {}
    virtual void windowObjectCleared() {}
    virtual void didPerformFirstNavigation() const {}

    virtual void registerForIconNotification(bool listen) {}

#if PLATFORM(MAC)
    virtual NSCachedURLResponse* willCacheResponse(DocumentLoader*, unsigned long identifier, NSCachedURLResponse* response) const { return response; }
#endif

};

class EmptyEditorClient : public EditorClient {
public:
    virtual ~EmptyEditorClient() { }
    virtual void pageDestroyed() { }

    virtual bool shouldDeleteRange(Range*) { return false; }
    virtual bool shouldShowDeleteInterface(HTMLElement*) { return false; }
    virtual bool smartInsertDeleteEnabled() { return false; }
    virtual bool isContinuousSpellCheckingEnabled() { return false; }
    virtual void toggleContinuousSpellChecking() { }
    virtual bool isGrammarCheckingEnabled() { return false; }
    virtual void toggleGrammarChecking() { }
    virtual int spellCheckerDocumentTag() { return -1; }

    virtual bool selectWordBeforeMenuEvent() { return false; }
    virtual bool isEditable() { return false; }

    virtual bool shouldBeginEditing(Range*) { return false; }
    virtual bool shouldEndEditing(Range*) { return false; }
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) { return false; }
    //  virtual bool shouldInsertNode(Node*, Range* replacingRange, WebViewInsertAction) { return false; }
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) { return false; }
    virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity, bool stillSelecting) { return false; }

    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*) { return false; }
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*) { return false; }
    //  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle) { return false; }
    //  virtual bool doCommandBySelector(SEL selector) { return false; }
    //
    virtual void didBeginEditing() { }
    virtual void respondToChangedContents() { }
    virtual void respondToChangedSelection() { }
    virtual void didEndEditing() { }
    virtual void didWriteSelectionToPasteboard() { }
    virtual void didSetSelectionTypesForPasteboard() { }
    //  virtual void webViewDidChangeTypingStyle:(NSNotification *)notification { }
    //  virtual void webViewDidChangeSelection:(NSNotification *)notification { }
    //  virtual NSUndoManager* undoManagerForWebView:(WebView *)webView { return 0; }

    virtual void registerCommandForUndo(PassRefPtr<EditCommand>) { }
    virtual void registerCommandForRedo(PassRefPtr<EditCommand>) { }
    virtual void clearUndoRedoOperations() { }

    virtual bool canUndo() const { return false; }
    virtual bool canRedo() const { return false; }

    virtual void undo() { }
    virtual void redo() { }

    virtual void handleKeyboardEvent(KeyboardEvent*) { }
    virtual void handleInputMethodKeydown(KeyboardEvent*) { }

    virtual void textFieldDidBeginEditing(Element*) { }
    virtual void textFieldDidEndEditing(Element*) { }
    virtual void textDidChangeInTextField(Element*) { }
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) { return false; }
    virtual void textWillBeDeletedInTextField(Element*) { }
    virtual void textDidChangeInTextArea(Element*) { }

#if PLATFORM(MAC)
    virtual void markedTextAbandoned(Frame*) { }

    virtual NSString* userVisibleString(NSURL*) { return 0; }
#ifdef BUILDING_ON_TIGER
    virtual NSArray* pasteboardTypesForSelection(Frame*) { return 0; }
#endif
#endif
    virtual void ignoreWordInSpellDocument(const String&) { }
    virtual void learnWord(const String&) { }
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength) { }
    virtual void checkGrammarOfString(const UChar*, int length, Vector<GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) { }
    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) { }
    virtual void updateSpellingUIWithMisspelledWord(const String&) { }
    virtual void showSpellingUI(bool show) { }
    virtual bool spellingUIIsShowing() { return false; }
    virtual void getGuessesForWord(const String&, Vector<String>& guesses) { }
    virtual void setInputMethodState(bool enabled) { }


};

class EmptyContextMenuClient : public ContextMenuClient {
public:
    virtual ~EmptyContextMenuClient() {  }
    virtual void contextMenuDestroyed() { }

    virtual PlatformMenuDescription getCustomMenuFromDefaultItems(ContextMenu*) { return 0; }
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*) { }

    virtual void downloadURL(const KURL& url) { }
    virtual void copyImageToClipboard(const HitTestResult&) { }
    virtual void searchWithGoogle(const Frame*) { }
    virtual void lookUpInDictionary(Frame*) { }
    virtual void speak(const String&) { }
    virtual void stopSpeaking() { }

#if PLATFORM(MAC)
    virtual void searchWithSpotlight() { }
#endif
};

class EmptyDragClient : public DragClient {
public:
    virtual ~EmptyDragClient() {}
    virtual void willPerformDragDestinationAction(DragDestinationAction, DragData*) { }
    virtual void willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard*) { }
    virtual DragDestinationAction actionMaskForDrag(DragData*) { return DragDestinationActionNone; }
    virtual DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) { return DragSourceActionNone; }
    virtual void startDrag(DragImageRef, const IntPoint&, const IntPoint&, Clipboard*, Frame*, bool) { }
    virtual DragImageRef createDragImageForLink(KURL&, const String& label, Frame*) { return 0; }
    virtual void dragControllerDestroyed() { }
};

class EmptyInspectorClient : public InspectorClient {
public:
    virtual ~EmptyInspectorClient() { }

    virtual void inspectorDestroyed() { }

    virtual Page* createPage() { return 0; };

    virtual String localizedStringsURL() { return String(); }

    virtual void showWindow() { }
    virtual void closeWindow() { }

    virtual void attachWindow() { }
    virtual void detachWindow() { }

    virtual void setAttachedWindowHeight(unsigned) { }

    virtual void highlight(Node*) { }
    virtual void hideHighlight() { }
    virtual void inspectedURLChanged(const String& newURL) { }

    virtual void populateSetting(const String& key, InspectorController::Setting&) { }
    virtual void storeSetting(const String& key, const InspectorController::Setting&) { }
    virtual void removeSetting(const String& key) { }
};

}

#endif // EmptyClients_h
