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

#ifndef SVGImageEmptyClients_H
#define SVGImageEmptyClients_H

#ifdef SVG_SUPPORT

#include "ChromeClient.h"
#include "ContextMenuClient.h"
#include "EditorClient.h"
#include "FrameLoaderClient.h"

/*
 This file holds empty Client stubs for use by SVGImage.
 SVGImage needs to create a dummy Page->Frame->FrameView tree for use in parsing an SVGDocument.
 This tree depends heavily on Clients (usually provided by WebKit classes).
 
 SVGImage has no way to access the current Page (nor should it, since Images are not tied to a page).
 See http://bugs.webkit.org/show_bug.cgi?id=5971 for more discussion on this issue.
 
 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes.  Unfortunate, yes.  Hopefully temporary.
*/

namespace WebCore {

class SVGEmptyCromeClient : public ChromeClient {
public:
    virtual ~SVGEmptyCromeClient() { }
    virtual void chromeDestroyed() { }
    
    virtual void setWindowRect(const FloatRect&) { }
    virtual FloatRect windowRect() { return FloatRect(); }
    
    virtual FloatRect pageRect() { return FloatRect(); }
    
    virtual float scaleFactor() { return 1f; }
    
    virtual void focus() { }
    virtual void unfocus() { }
    
    virtual Page* createWindow(const FrameLoadRequest&) { return 0; }
    virtual Page* createModalDialog(const FrameLoadRequest&) { return 0; }
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
};

class SVGEmptyFrameLoaderClient : public FrameLoaderClient {
public:
    virtual ~SVGEmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() { }
    
    virtual bool hasWebView() const { return true; } // mainly for assertions
    virtual bool hasFrameView() const { return true; } // ditto
    
    virtual bool hasBackForwardList() const { return false; }
    virtual void resetBackForwardList() { }
    
    virtual bool provisionalItemIsTarget() const { return false; }
    virtual bool loadProvisionalItemFromPageCache() { return false; }
    virtual void invalidateCurrentItemPageCache() { }
    
    virtual bool privateBrowsingEnabled() const { return false; }
    
    virtual void makeDocumentView() { }
    virtual void makeRepresentation(DocumentLoader*) { }
#if PLATFORM(MAC)
    virtual void setDocumentViewFromPageCache(NSDictionary *) { }
#endif
    virtual void forceLayout() { }
    virtual void forceLayoutForNonHTML() { }
    
    virtual void updateHistoryForCommit() { }
    
    virtual void updateHistoryForBackForwardNavigation() { }
    virtual void updateHistoryForReload() { }
    virtual void updateHistoryForStandardLoad() { }
    virtual void updateHistoryForInternalLoad() { }
    
    virtual void updateHistoryAfterClientRedirect() { }
    
    virtual void setCopiesOnScroll() { }
    
    virtual LoadErrorResetToken* tokenForLoadErrorReset() { return 0; }
    virtual void resetAfterLoadError(LoadErrorResetToken*) { }
    virtual void doNotResetAfterLoadError(LoadErrorResetToken*) { }
    
    virtual void detachedFromParent1() { }
    virtual void detachedFromParent2() { }
    virtual void detachedFromParent3() { }
    virtual void detachedFromParent4() { }
    
    virtual void loadedFromPageCache() { }
    
#if PLATFORM(MAC)
    virtual void download(ResourceHandle*, NSURLRequest *, NSURLResponse *) { }
    
    virtual id dispatchIdentifierForInitialRequest(DocumentLoader*, const ResourceRequest&) { return 0; }
    virtual NSURLRequest *dispatchWillSendRequest(DocumentLoader*, id identifier, NSURLRequest *, NSURLResponse *redirectResponse) { return 0; }
    virtual void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) { }
    virtual void dispatchDidCancelAuthenticationChallenge(DocumentLoader*, id identifier, NSURLAuthenticationChallenge *) { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, id identifier, NSURLResponse *) { }
    virtual void dispatchDidReceiveContentLength(DocumentLoader*, id identifier, int lengthReceived) { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, id identifier) { }
    virtual void dispatchDidFailLoading(DocumentLoader*, id identifier, const ResourceError&) { }
    virtual bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, NSURLRequest *, NSURLResponse *, int length) { return false; }
#endif
    
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
#if PLATFORM(MAC)
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) { }
    virtual void dispatchDidFailLoad(const ResourceError&) { }
#endif
    virtual void dispatchDidFinishLoad() { }
    virtual void dispatchDidFirstLayout() { }
    
#if PLATFORM(MAC)
    virtual Frame* dispatchCreatePage(NSURLRequest *) { return 0; }
#endif
    virtual void dispatchShow() { }
    
#if PLATFORM(MAC)
    virtual void dispatchDecidePolicyForMIMEType(FramePolicyFunction, const String& MIMEType, const ResourceRequest&) { }
    virtual void dispatchDecidePolicyForNewWindowAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&, const String& frameName) { }
    virtual void dispatchDecidePolicyForNavigationAction(FramePolicyFunction, const NavigationAction&, const ResourceRequest&) { }
#endif
    virtual void cancelPolicyCheck() { }
    
#if PLATFORM(MAC)
    virtual void dispatchUnableToImplementPolicy(const ResourceError&) { }
#endif
    
    virtual void dispatchWillSubmitForm(FramePolicyFunction, PassRefPtr<FormState>) { }
    
    virtual void dispatchDidLoadMainResource(DocumentLoader*) { }
    virtual void clearLoadingFromPageCache(DocumentLoader*) { }
    virtual bool isLoadingFromPageCache(DocumentLoader*) { return 0; }
    virtual void revertToProvisionalState(DocumentLoader*) { }
#if PLATFORM(MAC)
    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) { }
#endif
    virtual void clearUnarchivingState(DocumentLoader*) { }
    
    virtual void progressStarted() { }
    virtual void progressCompleted() { }
    
#if PLATFORM(MAC)
    virtual void incrementProgress(id identifier, NSURLResponse *) { }
    virtual void incrementProgress(id identifier, const char*, int) { }
    virtual void completeProgress(id identifier) { }
#endif
    
    virtual void setMainFrameDocumentReady(bool) { }
    
#if PLATFORM(MAC)
    virtual void startDownload(const ResourceRequest&) { }
#endif
    
    virtual void willChangeTitle(DocumentLoader*) { }
    virtual void didChangeTitle(DocumentLoader*) { }
    
#if PLATFORM(MAC)
    virtual void committedLoad(DocumentLoader*, const char*, int) { }
#endif
    virtual void finishedLoading(DocumentLoader*) { }
    virtual void finalSetupForReplace(DocumentLoader*) { }
    
#if PLATFORM(MAC)
    virtual ResourceError cancelledError(const ResourceRequest&) { return ResourceError(); }
    virtual ResourceError cannotShowURLError(const ResourceRequest&) { return ResourceError(); }
    virtual ResourceError interruptForPolicyChangeError(const ResourceRequest&) { return ResourceError(); }
    
    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) { return ResourceError(); }
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) { return ResourceError(); }
    
    virtual bool shouldFallBack(const ResourceError&) { return false; }
#endif
    
    virtual void setDefersLoading(bool) { }
    
#if PLATFORM(MAC)
    virtual bool willUseArchive(ResourceLoader*, NSURLRequest *, const KURL& originalURL) const { return false; }
#endif
    virtual bool isArchiveLoadPending(ResourceLoader*) const { return false; }
    virtual void cancelPendingArchiveLoad(ResourceLoader*) { }
    virtual void clearArchivedResources() { }
    
    virtual bool canHandleRequest(const ResourceRequest&) const { return false; }
    virtual bool canShowMIMEType(const String& MIMEType) const { return false; }
    virtual bool representationExistsForURLScheme(const String& URLScheme) const { return false; }
    virtual String generatedMIMETypeForURLScheme(const String& URLScheme) const { return ""; }
    
    virtual void frameLoadCompleted() { }
    virtual void restoreScrollPositionAndViewState() { }
    virtual void provisionalLoadStarted() { }
    virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const { return false; }
    virtual void addHistoryItemForFragmentScroll() { }
    virtual void didFinishLoad() { }
    virtual void prepareForDataSourceReplacement() { }
    
#if PLATFORM(MAC)
    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest& request) { return new DocumentLoader(request); }
#endif
    virtual void setTitle(const String& title, const KURL&) { }
    
    virtual String userAgent() { return ""; }
};

class SVGEmptyEditorClient : public EditorClient {
public:
    virtual ~SVGEmptyEditorClient() {  }
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
    virtual bool shouldInsertText(String, Range*, EditorInsertAction) { return false; }
    //  virtual bool shouldChangeSelectedRange(Range* fromRange, Range* toRange, NSSelectionAffinity, bool stillSelecting) { return false; }
    virtual bool shouldApplyStyle(CSSStyleDeclaration*, Range*) { return false; }
    //  virtual bool shouldChangeTypingStyle(CSSStyleDeclaration* fromStyle, CSSStyleDeclaration* toStyle) { return false; }
    //  virtual bool doCommandBySelector(SEL selector) { return false; }
    //
    virtual void didBeginEditing() { }
    virtual void respondToChangedContents() { }
    virtual void didEndEditing() { }
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
    
#if PLATFORM(MAC)
    // FIXME: This should become SelectionController::toWebArchive()
    virtual NSData* dataForArchivedSelection(Frame*) { return 0; } 
    
    virtual NSString* userVisibleString(NSURL*) { return 0; }
#endif
    
};

class SVGEmptyContextMenuClient : public ContextMenuClient {
public:
    virtual ~SVGEmptyContextMenuClient() {  }
    virtual void contextMenuDestroyed() { }
    
    virtual void addCustomContextMenuItems(ContextMenu*) { }
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*) { }
    
    virtual void downloadURL(const KURL& url) { }
    virtual void copyImageToClipboard(const HitTestResult&) { }
    virtual void lookUpInDictionary(Frame*) { }
    virtual void speak(const String&) { }
    virtual void stopSpeaking() { }
    
#if PLATFORM(MAC)
    virtual void searchWithSpotlight() { }
#endif
};

}

#endif // SVG_SUPPORT

#endif // SVGImageEmptyClients_H

