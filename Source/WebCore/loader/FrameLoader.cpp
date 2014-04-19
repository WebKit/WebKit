/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoader.h"

#include "AXObjectCache.h"
#include "ApplicationCacheHost.h"
#include "BackForwardController.h"
#include "BeforeUnloadEvent.h"
#include "CachedPage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContentSecurityPolicy.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "DatabaseManager.h"
#include "Document.h"
#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FormState.h"
#include "FormSubmission.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClient.h"
#include "FrameNetworkingContext.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "HTTPParsers.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "IconController.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageActivityAssertionToken.h"
#include "PageCache.h"
#include "PageThrottler.h"
#include "PageTransitionEvent.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "PluginDatabase.h"
#include "PluginDocument.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SVGDocument.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "SchemeRegistry.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScrollAnimator.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "SegmentedString.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "TextResourceDecoder.h"
#include "WindowFeatures.h"
#include "XMLDocumentParser.h"
#include <wtf/CurrentTime.h>
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SHARED_WORKERS)
#include "SharedWorkerRepository.h"
#endif

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "Archive.h"
#endif

#if PLATFORM(IOS)
#include "DocumentType.h"
#include "MemoryPressureHandler.h"
#include "ResourceLoader.h"
#include "RuntimeApplicationChecksIOS.h"
#include "SystemMemory.h"
#include "WKContentObservation.h"
#endif

namespace WebCore {

using namespace HTMLNames;
using namespace SVGNames;

static const char defaultAcceptHeader[] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

#if PLATFORM(IOS)
const int memoryLevelThresholdToPrunePageCache = 20;
#endif

bool isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
        case FrameLoadTypeStandard:
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeRedirectWithLockedBackForwardList:
        case FrameLoadTypeReplace:
            return false;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// This is not in the FrameLoader class to emphasize that it does not depend on
// private FrameLoader data, and to avoid increasing the number of public functions
// with access to private data.  Since only this .cpp file needs it, making it
// non-member lets us exclude it from the header file, thus keeping FrameLoader.h's
// API simpler.
//
static bool isDocumentSandboxed(Frame* frame, SandboxFlags mask)
{
    return frame->document() && frame->document()->isSandboxed(mask);
}

class FrameLoader::FrameProgressTracker {
public:
    explicit FrameProgressTracker(Frame& frame)
        : m_frame(frame)
        , m_inProgress(false)
    {
    }

    ~FrameProgressTracker()
    {
        ASSERT(!m_inProgress || m_frame.page());
        if (m_inProgress)
            m_frame.page()->progress().progressCompleted(m_frame);
    }

    void progressStarted()
    {
        ASSERT(m_frame.page());
        if (!m_inProgress)
            m_frame.page()->progress().progressStarted(m_frame);
        m_inProgress = true;
    }

    void progressCompleted()
    {
        ASSERT(m_inProgress);
        ASSERT(m_frame.page());
        m_inProgress = false;
        m_frame.page()->progress().progressCompleted(m_frame);
    }

private:
    Frame& m_frame;
    bool m_inProgress;
};

FrameLoader::FrameLoader(Frame& frame, FrameLoaderClient& client)
    : m_frame(frame)
    , m_client(client)
    , m_policyChecker(std::make_unique<PolicyChecker>(frame))
    , m_history(std::make_unique<HistoryController>(frame))
    , m_notifier(frame)
    , m_subframeLoader(std::make_unique<SubframeLoader>(frame))
    , m_icon(std::make_unique<IconController>(frame))
    , m_mixedContentChecker(frame)
    , m_state(FrameStateProvisional)
    , m_loadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_inStopAllLoaders(false)
    , m_isExecutingJavaScriptFormAction(false)
    , m_didCallImplicitClose(true)
    , m_wasUnloadEventEmitted(false)
    , m_pageDismissalEventBeingDispatched(NoDismissal)
    , m_isComplete(false)
    , m_needsClear(false)
    , m_checkTimer(this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_shouldCallCheckLoadComplete(false)
    , m_opener(nullptr)
    , m_loadingFromCachedPage(false)
    , m_suppressOpenerInNewFrame(false)
    , m_currentNavigationHasShownBeforeUnloadConfirmPanel(false)
    , m_loadsSynchronously(false)
    , m_forcedSandboxFlags(SandboxNone)
{
}

FrameLoader::~FrameLoader()
{
    setOpener(nullptr);

    HashSet<Frame*>::iterator end = m_openedFrames.end();
    for (HashSet<Frame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->loader().m_opener = 0;

    m_client.frameLoaderDestroyed();

    if (m_networkingContext)
        m_networkingContext->invalidate();
}

void FrameLoader::init()
{
    // This somewhat odd set of steps gives the frame an initial empty document.
    setPolicyDocumentLoader(m_client.createDocumentLoader(ResourceRequest(URL(ParsedURLString, emptyString())), SubstituteData()).get());
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_provisionalDocumentLoader->startLoadingMainResource();
    m_frame.document()->cancelParsing();
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);

    m_networkingContext = m_client.createNetworkingContext();
    m_progressTracker = std::make_unique<FrameProgressTracker>(m_frame);
}

#if PLATFORM(IOS)
void FrameLoader::initForSynthesizedDocument(const URL&)
{
    // FIXME: We need to initialize the document URL to the specified URL. Currently the URL is empty and hence
    // FrameLoader::checkCompleted() will overwrite the URL of the document to be activeDocumentLoader()->documentURL().

    RefPtr<DocumentLoader> loader = m_client.createDocumentLoader(ResourceRequest(URL(ParsedURLString, emptyString())), SubstituteData());
    loader->setFrame(&m_frame);
    loader->setResponse(ResourceResponse(URL(), ASCIILiteral("text/html"), 0, String(), String()));
    loader->setCommitted(true);
    setDocumentLoader(loader.get());

    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
    m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    m_client.transitionToCommittedForNewPage();

    m_didCallImplicitClose = true;
    m_isComplete = true;
    m_state = FrameStateComplete;
    m_needsClear = true;

    m_networkingContext = m_client.createNetworkingContext();
    m_progressTracker = std::make_unique<FrameProgressTracker>(m_frame);
}
#endif

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_documentLoader)
        m_documentLoader->setDefersLoading(defers);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->setDefersLoading(defers);
    if (m_policyDocumentLoader)
        m_policyDocumentLoader->setDefersLoading(defers);
    history().setDefersLoading(defers);

    if (!defers) {
        m_frame.navigationScheduler().startTimer();
        startCheckCompleteTimer();
    }
}

void FrameLoader::changeLocation(SecurityOrigin* securityOrigin, const URL& url, const String& referrer, LockHistory lockHistory, LockBackForwardList lockBackForwardList, bool refresh)
{
    urlSelected(FrameLoadRequest(securityOrigin, ResourceRequest(url, referrer, refresh ? ReloadIgnoringCacheData : UseProtocolCachePolicy), "_self"),
        0, lockHistory, lockBackForwardList, MaybeSendReferrer, ReplaceDocumentIfJavaScriptURL);
}

void FrameLoader::urlSelected(const URL& url, const String& passedTarget, PassRefPtr<Event> triggeringEvent, LockHistory lockHistory, LockBackForwardList lockBackForwardList, ShouldSendReferrer shouldSendReferrer)
{
    urlSelected(FrameLoadRequest(m_frame.document()->securityOrigin(), ResourceRequest(url), passedTarget),
        triggeringEvent, lockHistory, lockBackForwardList, shouldSendReferrer, DoNotReplaceDocumentIfJavaScriptURL);
}

// The shouldReplaceDocumentIfJavaScriptURL parameter will go away when the FIXME to eliminate the
// corresponding parameter from ScriptController::executeIfJavaScriptURL() is addressed.
void FrameLoader::urlSelected(const FrameLoadRequest& passedRequest, PassRefPtr<Event> triggeringEvent, LockHistory lockHistory, LockBackForwardList lockBackForwardList, ShouldSendReferrer shouldSendReferrer, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL)
{
    ASSERT(!m_suppressOpenerInNewFrame);

    Ref<Frame> protect(m_frame);
    FrameLoadRequest frameRequest(passedRequest);

    if (m_frame.script().executeIfJavaScriptURL(frameRequest.resourceRequest().url(), shouldReplaceDocumentIfJavaScriptURL))
        return;

    if (frameRequest.frameName().isEmpty())
        frameRequest.setFrameName(m_frame.document()->baseTarget());

    if (shouldSendReferrer == NeverSendReferrer)
        m_suppressOpenerInNewFrame = true;
    addHTTPOriginIfNeeded(frameRequest.resourceRequest(), outgoingOrigin());

    loadFrameRequest(frameRequest, lockHistory, lockBackForwardList, triggeringEvent, 0, shouldSendReferrer);

    m_suppressOpenerInNewFrame = false;
}

void FrameLoader::submitForm(PassRefPtr<FormSubmission> submission)
{
    ASSERT(submission->method() == FormSubmission::PostMethod || submission->method() == FormSubmission::GetMethod);

    // FIXME: Find a good spot for these.
    ASSERT(submission->data());
    ASSERT(submission->state());
    ASSERT(!submission->state()->sourceDocument()->frame() || submission->state()->sourceDocument()->frame() == &m_frame);
    
    if (!m_frame.page())
        return;
    
    if (submission->action().isEmpty())
        return;

    if (isDocumentSandboxed(&m_frame, SandboxForms)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked form submission to '" + submission->action().stringCenterEllipsizedToLength() + "' because the form's frame is sandboxed and the 'allow-forms' permission is not set.");
        return;
    }

    if (protocolIsJavaScript(submission->action())) {
        if (!m_frame.document()->contentSecurityPolicy()->allowFormAction(URL(submission->action())))
            return;
        m_isExecutingJavaScriptFormAction = true;
        m_frame.script().executeIfJavaScriptURL(submission->action(), DoNotReplaceDocumentIfJavaScriptURL);
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    Frame* targetFrame = findFrameForNavigation(submission->target(), submission->state()->sourceDocument());
    if (!targetFrame) {
        if (!DOMWindow::allowPopUp(&m_frame) && !ScriptController::processingUserGesture())
            return;

        // FIXME: targetFrame can be 0 for two distinct reasons:
        // 1. The frame was not found by name, so we should try opening a new window.
        // 2. The frame was found, but navigating it was not allowed, e.g. by HTML5 sandbox or by origin checks.
        // Continuing form submission makes no sense in the latter case.
        // There is a repeat check after timer fires, so this is not a correctness issue.

        targetFrame = &m_frame;
    } else
        submission->clearTarget();

    if (!targetFrame->page())
        return;

    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.

    // We do not want to submit more than one form from the same page, nor do we want to submit a single
    // form more than once. This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.

    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.

    if (m_frame.tree().isDescendantOf(targetFrame)) {
        if (m_submittedFormURL == submission->requestURL())
            return;
        m_submittedFormURL = submission->requestURL();
    }

    submission->data()->generateFiles(m_frame.document());
    submission->setReferrer(outgoingReferrer());
    submission->setOrigin(outgoingOrigin());

    targetFrame->navigationScheduler().scheduleFormSubmission(submission);
}

void FrameLoader::stopLoading(UnloadEventPolicy unloadEventPolicy)
{
    if (m_frame.document() && m_frame.document()->parser())
        m_frame.document()->parser()->stopParsing();

    if (unloadEventPolicy != UnloadEventPolicyNone) {
        if (m_frame.document()) {
            if (m_didCallImplicitClose && !m_wasUnloadEventEmitted) {
                Element* currentFocusedElement = m_frame.document()->focusedElement();
                if (currentFocusedElement && currentFocusedElement->toInputElement())
                    currentFocusedElement->toInputElement()->endEditing();
                if (m_pageDismissalEventBeingDispatched == NoDismissal) {
                    if (unloadEventPolicy == UnloadEventPolicyUnloadAndPageHide) {
                        m_pageDismissalEventBeingDispatched = PageHideDismissal;
                        m_frame.document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, m_frame.document()->inPageCache()), m_frame.document());
                    }

                    // FIXME: update Page Visibility state here.
                    // https://bugs.webkit.org/show_bug.cgi?id=116770

                    if (!m_frame.document()->inPageCache()) {
                        RefPtr<Event> unloadEvent(Event::create(eventNames().unloadEvent, false, false));
                        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed
                        // while dispatching the event, so protect it to prevent writing the end
                        // time into freed memory.
                        RefPtr<DocumentLoader> documentLoader = m_provisionalDocumentLoader;
                        m_pageDismissalEventBeingDispatched = UnloadDismissal;
                        if (documentLoader && !documentLoader->timing()->unloadEventStart() && !documentLoader->timing()->unloadEventEnd()) {
                            DocumentLoadTiming* timing = documentLoader->timing();
                            ASSERT(timing->navigationStart());
                            timing->markUnloadEventStart();
                            m_frame.document()->domWindow()->dispatchEvent(unloadEvent, m_frame.document());
                            timing->markUnloadEventEnd();
                        } else
                            m_frame.document()->domWindow()->dispatchEvent(unloadEvent, m_frame.document());
                    }
                }
                m_pageDismissalEventBeingDispatched = NoDismissal;
                if (m_frame.document())
                    m_frame.document()->updateStyleIfNeeded();
                m_wasUnloadEventEmitted = true;
            }
        }

        // Dispatching the unload event could have made m_frame.document() null.
        if (m_frame.document() && !m_frame.document()->inPageCache()) {
            // Don't remove event listeners from a transitional empty document (see bug 28716 for more information).
            bool keepEventListeners = m_stateMachine.isDisplayingInitialEmptyDocument() && m_provisionalDocumentLoader
                && m_frame.document()->isSecureTransitionTo(m_provisionalDocumentLoader->url());

            if (!keepEventListeners)
                m_frame.document()->removeAllEventListeners();
        }
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing()
    m_didCallImplicitClose = true; // don't want that one either

    if (m_frame.document() && m_frame.document()->parsing()) {
        finishedParsing();
        m_frame.document()->setParsing(false);
    }

    if (Document* doc = m_frame.document()) {
        // FIXME: HTML5 doesn't tell us to set the state to complete when aborting, but we do anyway to match legacy behavior.
        // http://www.w3.org/Bugs/Public/show_bug.cgi?id=10537
        doc->setReadyState(Document::Complete);

#if ENABLE(SQL_DATABASE)
        // FIXME: Should the DatabaseManager watch for something like ActiveDOMObject::stop() rather than being special-cased here?
        DatabaseManager::manager().stopDatabases(doc, 0);
#endif
    }

    // FIXME: This will cancel redirection timer, which really needs to be restarted when restoring the frame from b/f cache.
    m_frame.navigationScheduler().cancel();
}

void FrameLoader::stop()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it will be deleted by checkCompleted().
    Ref<Frame> protect(m_frame);

    if (DocumentParser* parser = m_frame.document()->parser()) {
        parser->stopParsing();
        parser->finish();
    }
    
    icon().stopLoader();
}

void FrameLoader::willTransitionToCommitted()
{
    // This function is called when a frame is still fully in place (not cached, not detached), but will be replaced.

    if (m_frame.editor().hasComposition()) {
        // The text was already present in DOM, so it's better to confirm than to cancel the composition.
        m_frame.editor().confirmComposition();
        if (EditorClient* editorClient = m_frame.editor().client())
            editorClient->respondToChangedSelection(&m_frame);
    }
}

bool FrameLoader::closeURL()
{
    history().saveDocumentState();
    
    // Should only send the pagehide event here if the current document exists and has not been placed in the page cache.    
    Document* currentDocument = m_frame.document();
    stopLoading(currentDocument && !currentDocument->inPageCache() ? UnloadEventPolicyUnloadAndPageHide : UnloadEventPolicyUnloadOnly);
    
    m_frame.editor().clearUndoRedoOperations();
    return true;
}

bool FrameLoader::didOpenURL()
{
    if (m_frame.navigationScheduler().redirectScheduledDuringLoad()) {
        // A redirect was scheduled before the document was created.
        // This can happen when one frame changes another frame's location.
        return false;
    }

    m_frame.navigationScheduler().cancel();
    m_frame.editor().clearLastEditCommand();

    m_isComplete = false;
    m_didCallImplicitClose = false;

    // If we are still in the process of initializing an empty document then
    // its frame is not in a consistent state for rendering, so avoid setJSStatusBarText
    // since it may cause clients to attempt to render the frame.
    if (!m_stateMachine.creatingInitialEmptyDocument()) {
        DOMWindow* window = m_frame.document()->domWindow();
        window->setStatus(String());
        window->setDefaultStatus(String());
    }

    started();

    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;
    m_didCallImplicitClose = false;

    // Calling document.open counts as committing the first real document load.
    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
    
    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call. 
    // Canceling redirection here works for all cases because document.open 
    // implicitly precedes document.write.
    m_frame.navigationScheduler().cancel();
}


void FrameLoader::cancelAndClear()
{
    m_frame.navigationScheduler().cancel();

    if (!m_isComplete)
        closeURL();

    clear(m_frame.document(), false);
    m_frame.script().updatePlatformScriptObjects();
}

void FrameLoader::clear(Document* newDocument, bool clearWindowProperties, bool clearScriptObjects, bool clearFrameView)
{
    m_frame.editor().clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;
    
    if (!m_frame.document()->inPageCache()) {
        m_frame.document()->cancelParsing();
        m_frame.document()->stopActiveDOMObjects();
        if (m_frame.document()->hasLivingRenderTree()) {
            m_frame.document()->prepareForDestruction();
            m_frame.document()->removeFocusedNodeOfSubtree(m_frame.document());
        }
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        InspectorInstrumentation::frameWindowDiscarded(&m_frame, m_frame.document()->domWindow());
        m_frame.document()->domWindow()->resetUnlessSuspendedForPageCache();
        m_frame.script().clearWindowShell(newDocument->domWindow(), m_frame.document()->inPageCache());
    }

    m_frame.selection().prepareForDestruction();
    m_frame.eventHandler().clear();
    if (clearFrameView && m_frame.view())
        m_frame.view()->clear();

    // Do not drop the document before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame.setDocument(0);

    subframeLoader().clear();

    if (clearScriptObjects)
        m_frame.script().clearScriptObjects();

    m_frame.script().enableEval();

    m_frame.navigationScheduler().clear();

    m_checkTimer.stop();
    m_shouldCallCheckCompleted = false;
    m_shouldCallCheckLoadComplete = false;

    if (m_stateMachine.isDisplayingInitialEmptyDocument() && m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
}

void FrameLoader::receivedFirstData()
{
    dispatchDidCommitLoad();
    dispatchDidClearWindowObjectsInAllWorlds();
    dispatchGlobalObjectAvailableInAllWorlds();

    if (m_documentLoader) {
        StringWithDirection ptitle = m_documentLoader->title();
        // If we have a title let the WebView know about it.
        if (!ptitle.isNull())
            m_client.dispatchDidReceiveTitle(ptitle);
    }

    if (!m_documentLoader)
        return;

    double delay;
    String url;
    if (!parseHTTPRefresh(m_documentLoader->response().httpHeaderField("Refresh"), false, delay, url))
        return;
    if (url.isEmpty())
        url = m_frame.document()->url().string();
    else
        url = m_frame.document()->completeURL(url).string();

    if (!protocolIsJavaScript(url))
        m_frame.navigationScheduler().scheduleRedirect(delay, url);
    else {
        String message = "Refused to refresh " + m_frame.document()->url().stringCenterEllipsizedToLength() + " to a javascript: URL";
        m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
    }
}

void FrameLoader::setOutgoingReferrer(const URL& url)
{
    m_outgoingReferrer = url.strippedForUseAsReferrer();
}

void FrameLoader::didBeginDocument(bool dispatch)
{
    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_frame.document()->setReadyState(Document::Loading);

    if (m_pendingStateObject) {
        m_frame.document()->statePopped(m_pendingStateObject.get());
        m_pendingStateObject.clear();
    }

    if (dispatch)
        dispatchDidClearWindowObjectsInAllWorlds();

    updateFirstPartyForCookies();
    m_frame.document()->initContentSecurityPolicy();

    const Settings& settings = m_frame.settings();
    m_frame.document()->cachedResourceLoader()->setImagesEnabled(settings.areImagesEnabled());
    m_frame.document()->cachedResourceLoader()->setAutoLoadImages(settings.loadsImagesAutomatically());

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            m_frame.document()->parseDNSPrefetchControlHeader(dnsPrefetchControl);

        String policyValue = m_documentLoader->response().httpHeaderField("Content-Security-Policy");
        if (!policyValue.isEmpty())
            m_frame.document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::Enforce);

        policyValue = m_documentLoader->response().httpHeaderField("Content-Security-Policy-Report-Only");
        if (!policyValue.isEmpty())
            m_frame.document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::Report);

        policyValue = m_documentLoader->response().httpHeaderField("X-WebKit-CSP");
        if (!policyValue.isEmpty())
            m_frame.document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::PrefixedEnforce);

        policyValue = m_documentLoader->response().httpHeaderField("X-WebKit-CSP-Report-Only");
        if (!policyValue.isEmpty())
            m_frame.document()->contentSecurityPolicy()->didReceiveHeader(policyValue, ContentSecurityPolicy::PrefixedReport);

        String headerContentLanguage = m_documentLoader->response().httpHeaderField("Content-Language");
        if (!headerContentLanguage.isEmpty()) {
            size_t commaIndex = headerContentLanguage.find(',');
            headerContentLanguage.truncate(commaIndex); // notFound == -1 == don't truncate
            headerContentLanguage = headerContentLanguage.stripWhiteSpace(isHTMLSpace);
            if (!headerContentLanguage.isEmpty())
                m_frame.document()->setContentLanguage(headerContentLanguage);
        }
    }

    history().restoreDocumentState();
}

void FrameLoader::finishedParsing()
{
    m_frame.injectUserScripts(InjectAtDocumentEnd);

    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<Frame> protector = m_frame.view() ? &m_frame : 0;

    m_client.dispatchDidFinishDocumentLoad();

    checkCompleted();

    if (!m_frame.view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame.view()->restoreScrollbar();
    scrollToFragmentWithParentBoundary(m_frame.document()->url());
}

void FrameLoader::loadDone()
{
    checkCompleted();
}

bool FrameLoader::allChildrenAreComplete() const
{
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->loader().m_isComplete)
            return false;
    }
    return true;
}

bool FrameLoader::allAncestorsAreComplete() const
{
    for (Frame* ancestor = &m_frame; ancestor; ancestor = ancestor->tree().parent()) {
        if (!ancestor->loader().m_isComplete)
            return false;
    }
    return true;
}

void FrameLoader::checkCompleted()
{
    Ref<Frame> protect(m_frame);
    m_shouldCallCheckCompleted = false;

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame.document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (m_frame.document()->cachedResourceLoader()->requestCount())
        return;

    // Still waiting for elements that don't go through a FrameLoader?
    if (m_frame.document()->isDelayingLoadEvent())
        return;

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // OK, completed.
    m_isComplete = true;
    m_requestedHistoryItem = 0;
    m_frame.document()->setReadyState(Document::Complete);

#if PLATFORM(IOS)
    if (m_frame.document()->url().isEmpty()) {
        // We need to update the document URL of a PDF document to be non-empty so that both back/forward history navigation
        // between PDF pages and fragment navigation works. See <rdar://problem/9544769> for more details.
        // FIXME: Is there a better place for this code, say DocumentLoader? Also, we should explicitly only update the URL
        // of the document when it's a PDFDocument object instead of assuming that a Document object with an empty URL is a PDFDocument.
        // FIXME: This code is incorrect for a synthesized document (which also has an empty URL). The URL for a synthesized
        // document should be the URL specified to FrameLoader::initForSynthesizedDocument().
        m_frame.document()->setURL(activeDocumentLoader()->documentURL());
    }
#endif

    checkCallImplicitClose(); // if we didn't do it before

    m_frame.navigationScheduler().startTimer();

    completed();
    if (m_frame.page())
        checkLoadComplete();
}

void FrameLoader::checkTimerFired(Timer<FrameLoader>&)
{
    Ref<Frame> protect(m_frame);

    if (Page* page = m_frame.page()) {
        if (page->defersLoading())
            return;
    }
    if (m_shouldCallCheckCompleted)
        checkCompleted();
    if (m_shouldCallCheckLoadComplete)
        checkLoadComplete();
}

void FrameLoader::startCheckCompleteTimer()
{
    if (!(m_shouldCallCheckCompleted || m_shouldCallCheckLoadComplete))
        return;
    if (m_checkTimer.isActive())
        return;
    m_checkTimer.startOneShot(0);
}

void FrameLoader::scheduleCheckCompleted()
{
    m_shouldCallCheckCompleted = true;
    startCheckCompleteTimer();
}

void FrameLoader::scheduleCheckLoadComplete()
{
    m_shouldCallCheckLoadComplete = true;
    startCheckCompleteTimer();
}

void FrameLoader::checkCallImplicitClose()
{
    if (m_didCallImplicitClose || m_frame.document()->parsing() || m_frame.document()->isDelayingLoadEvent())
        return;

    if (!allChildrenAreComplete())
        return; // still got a frame running -> too early

    m_didCallImplicitClose = true;
    m_wasUnloadEventEmitted = false;
    m_frame.document()->implicitClose();
}

void FrameLoader::loadURLIntoChildFrame(const URL& url, const String& referer, Frame* childFrame)
{
    ASSERT(childFrame);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    RefPtr<Archive> subframeArchive = activeDocumentLoader()->popArchiveForSubframe(childFrame->tree().uniqueName(), url);
    if (subframeArchive) {
        childFrame->loader().loadArchive(subframeArchive.release());
        return;
    }
#endif // ENABLE(WEB_ARCHIVE)

    HistoryItem* parentItem = history().currentItem();
    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    if (parentItem && parentItem->children().size() && isBackForwardLoadType(loadType()) 
        && !m_frame.document()->loadEventFinished()) {
        HistoryItem* childItem = parentItem->childItemWithTarget(childFrame->tree().uniqueName());
        if (childItem) {
            childFrame->loader().m_requestedHistoryItem = childItem;
            childFrame->loader().loadDifferentDocumentItem(childItem, loadType(), MayAttemptCacheOnlyLoadForFormSubmissionItem);
            return;
        }
    }

    childFrame->loader().loadURL(url, referer, "_self", LockHistory::No, FrameLoadTypeRedirectWithLockedBackForwardList, 0, 0);
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
void FrameLoader::loadArchive(PassRefPtr<Archive> archive)
{
    ArchiveResource* mainResource = archive->mainResource();
    ASSERT(mainResource);
    if (!mainResource)
        return;
        
    SubstituteData substituteData(mainResource->data(), mainResource->mimeType(), mainResource->textEncoding(), URL());
    
    ResourceRequest request(mainResource->url());
#if PLATFORM(MAC)
    request.applyWebArchiveHackForMail();
#endif

    RefPtr<DocumentLoader> documentLoader = m_client.createDocumentLoader(request, substituteData);
    documentLoader->setArchive(archive.get());
    load(documentLoader.get());
}
#endif // ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

ObjectContentType FrameLoader::defaultObjectContentType(const URL& url, const String& mimeTypeIn, bool shouldPreferPlugInsForImages)
{
    String mimeType = mimeTypeIn;

    if (mimeType.isEmpty())
        mimeType = mimeTypeFromURL(url);

#if !PLATFORM(COCOA) && !PLATFORM(EFL) // Mac has no PluginDatabase, nor does EFL
    if (mimeType.isEmpty()) {
        String decodedPath = decodeURLEscapeSequences(url.path());
        mimeType = PluginDatabase::installedPlugins()->MIMETypeForExtension(decodedPath.substring(decodedPath.reverseFind('.') + 1));
    }
#endif

    if (mimeType.isEmpty())
        return ObjectContentFrame; // Go ahead and hope that we can display the content.

#if !PLATFORM(COCOA) && !PLATFORM(EFL) // Mac has no PluginDatabase, nor does EFL
    bool plugInSupportsMIMEType = PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType);
#else
    bool plugInSupportsMIMEType = false;
#endif

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return shouldPreferPlugInsForImages && plugInSupportsMIMEType ? WebCore::ObjectContentNetscapePlugin : WebCore::ObjectContentImage;

    if (plugInSupportsMIMEType)
        return WebCore::ObjectContentNetscapePlugin;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return WebCore::ObjectContentFrame;

    return WebCore::ObjectContentNone;
}

String FrameLoader::outgoingReferrer() const
{
    // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
    // for why we walk the parent chain for srcdoc documents.
    Frame* frame = &m_frame;
    while (frame->document()->isSrcdocDocument()) {
        frame = frame->tree().parent();
        // Srcdoc documents cannot be top-level documents, by definition,
        // because they need to be contained in iframes with the srcdoc.
        ASSERT(frame);
    }
    return frame->loader().m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    return m_frame.document()->securityOrigin()->toString();
}

bool FrameLoader::checkIfFormActionAllowedByCSP(const URL& url) const
{
    if (m_submittedFormURL.isEmpty())
        return true;

    return m_frame.document()->contentSecurityPolicy()->allowFormAction(url);
}

Frame* FrameLoader::opener()
{
    return m_opener;
}

void FrameLoader::setOpener(Frame* opener)
{
    if (m_opener && !opener)
        m_client.didDisownOpener();

    if (m_opener)
        m_opener->loader().m_openedFrames.remove(&m_frame);
    if (opener)
        opener->loader().m_openedFrames.add(&m_frame);
    m_opener = opener;

    if (m_frame.document())
        m_frame.document()->initSecurityContext();
}

// FIXME: This does not belong in FrameLoader!
void FrameLoader::handleFallbackContent()
{
    HTMLFrameOwnerElement* owner = m_frame.ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    toHTMLObjectElement(owner)->renderFallbackContent();
}

void FrameLoader::provisionalLoadStarted()
{
    if (m_stateMachine.firstLayoutDone())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    m_frame.navigationScheduler().cancel(true);
    m_client.provisionalLoadStarted();
}

void FrameLoader::resetMultipleFormSubmissionProtection()
{
    m_submittedFormURL = URL();
}

void FrameLoader::updateFirstPartyForCookies()
{
    if (m_frame.tree().parent())
        setFirstPartyForCookies(m_frame.tree().parent()->document()->firstPartyForCookies());
    else
        setFirstPartyForCookies(m_frame.document()->url());
}

void FrameLoader::setFirstPartyForCookies(const URL& url)
{
    for (Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext(&m_frame))
        frame->document()->setFirstPartyForCookies(url);
}

// This does the same kind of work that didOpenURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::loadInSameDocument(const URL& url, PassRefPtr<SerializedScriptValue> stateObject, bool isNewNavigation)
{
    // If we have a state object, we cannot also be a new navigation.
    ASSERT(!stateObject || (stateObject && !isNewNavigation));

    // Update the data source's request with the new URL to fake the URL change
    URL oldURL = m_frame.document()->url();
    m_frame.document()->setURL(url);
    setOutgoingReferrer(url);
    documentLoader()->replaceRequestURLForSameDocumentNavigation(url);
    if (isNewNavigation && !shouldTreatURLAsSameAsCurrent(url) && !stateObject) {
        // NB: must happen after replaceRequestURLForSameDocumentNavigation(), since we add 
        // based on the current request. Must also happen before we openURL and displace the 
        // scroll position, since adding the BF item will save away scroll state.
        
        // NB2: If we were loading a long, slow doc, and the user fragment navigated before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.

        history().updateBackForwardListForFragmentScroll();
    }
    
    bool hashChange = equalIgnoringFragmentIdentifier(url, oldURL) && url.fragmentIdentifier() != oldURL.fragmentIdentifier();
    
    history().updateForSameDocumentNavigation();

    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    if (hashChange)
        m_frame.eventHandler().stopAutoscrollTimer();
    
    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    started();

    // We need to scroll to the fragment whether or not a hash change occurred, since
    // the user might have scrolled since the previous navigation.
    scrollToFragmentWithParentBoundary(url);
    
    m_isComplete = false;
    checkCompleted();

    if (isNewNavigation) {
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for fragment
        // navigation we'll not go through a real load and reach Completed state.
        checkLoadComplete();
    }

    m_client.dispatchDidNavigateWithinPage();

    m_frame.document()->statePopped(stateObject ? stateObject : SerializedScriptValue::nullValue());
    m_client.dispatchDidPopStateWithinPage();
    
    if (hashChange) {
        m_frame.document()->enqueueHashchangeEvent(oldURL, url);
        m_client.dispatchDidChangeLocationWithinPage();
    }
    
    // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
    m_client.didFinishLoad();
}

bool FrameLoader::isComplete() const
{
    return m_isComplete;
}

void FrameLoader::completed()
{
    Ref<Frame> protect(m_frame);

    for (Frame* descendant = m_frame.tree().traverseNext(&m_frame); descendant; descendant = descendant->tree().traverseNext(&m_frame))
        descendant->navigationScheduler().startTimer();

    if (Frame* parent = m_frame.tree().parent())
        parent->loader().checkCompleted();

    if (m_frame.view())
        m_frame.view()->maintainScrollPositionAtAnchor(0);
    m_activityAssertion = nullptr;
}

void FrameLoader::started()
{
    if (m_frame.page() && m_frame.page()->pageThrottler())
        m_activityAssertion = m_frame.page()->pageThrottler()->pageLoadActivityToken();
    for (Frame* frame = &m_frame; frame; frame = frame->tree().parent())
        frame->loader().m_isComplete = false;
}

void FrameLoader::prepareForHistoryNavigation()
{
    // If there is no currentItem, but we still want to engage in 
    // history navigation we need to manufacture one, and update
    // the state machine of this frame to impersonate having
    // loaded it.
    RefPtr<HistoryItem> currentItem = history().currentItem();
    if (!currentItem) {
        currentItem = HistoryItem::create();
        currentItem->setLastVisitWasFailure(true);
        history().setCurrentItem(currentItem.get());
        m_frame.page()->backForward().setCurrentItem(currentItem.get());

        ASSERT(stateMachine().isDisplayingInitialEmptyDocument());
        stateMachine().advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
        stateMachine().advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    }
}

void FrameLoader::prepareForLoadStart()
{
    m_progressTracker->progressStarted();
    m_client.dispatchDidStartProvisionalLoad();

    if (AXObjectCache::accessibilityEnabled()) {
        if (AXObjectCache* cache = m_frame.document()->existingAXObjectCache()) {
            AXObjectCache::AXLoadingEvent loadingEvent = loadType() == FrameLoadTypeReload ? AXObjectCache::AXLoadingReloaded : AXObjectCache::AXLoadingStarted;
            cache->frameLoadingEventNotification(&m_frame, loadingEvent);
        }
    }
}

void FrameLoader::setupForReplace()
{
    m_client.revertToProvisionalState(m_documentLoader.get());
    setState(FrameStateProvisional);
    m_provisionalDocumentLoader = m_documentLoader;
    m_documentLoader = 0;
    detachChildren();
}

void FrameLoader::loadFrameRequest(const FrameLoadRequest& request, LockHistory lockHistory, LockBackForwardList lockBackForwardList,
    PassRefPtr<Event> event, PassRefPtr<FormState> formState, ShouldSendReferrer shouldSendReferrer)
{    
    // Protect frame from getting blown away inside dispatchBeforeLoadEvent in loadWithDocumentLoader.
    Ref<Frame> protect(m_frame);

    URL url = request.resourceRequest().url();

    ASSERT(m_frame.document());
    if (!request.requester()->canDisplay(url)) {
        reportLocalLoadFailed(&m_frame, url.stringCenterEllipsizedToLength());
        return;
    }

    String argsReferrer = request.resourceRequest().httpReferrer();
    if (argsReferrer.isEmpty())
        argsReferrer = outgoingReferrer();

    String referrer = SecurityPolicy::generateReferrerHeader(m_frame.document()->referrerPolicy(), url, argsReferrer);
    if (shouldSendReferrer == NeverSendReferrer)
        referrer = String();
    
    FrameLoadType loadType;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        loadType = FrameLoadTypeReload;
    else if (lockBackForwardList == LockBackForwardList::Yes)
        loadType = FrameLoadTypeRedirectWithLockedBackForwardList;
    else
        loadType = FrameLoadTypeStandard;

    if (request.resourceRequest().httpMethod() == "POST")
        loadPostRequest(request.resourceRequest(), referrer, request.frameName(), lockHistory, loadType, event, formState.get());
    else
        loadURL(request.resourceRequest().url(), referrer, request.frameName(), lockHistory, loadType, event, formState.get());

    // FIXME: It's possible this targetFrame will not be the same frame that was targeted by the actual
    // load if frame names have changed.
    Frame* sourceFrame = formState ? formState->sourceDocument()->frame() : &m_frame;
    if (!sourceFrame)
        sourceFrame = &m_frame;
    Frame* targetFrame = sourceFrame->loader().findFrameForNavigation(request.frameName());
    if (targetFrame && targetFrame != sourceFrame) {
        if (Page* page = targetFrame->page())
            page->chrome().focus();
    }
}

void FrameLoader::loadURL(const URL& newURL, const String& referrer, const String& frameName, LockHistory lockHistory, FrameLoadType newLoadType,
    PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
{
    if (m_inStopAllLoaders)
        return;

    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;
    
    ResourceRequest request(newURL);
    if (!referrer.isEmpty()) {
        request.setHTTPReferrer(referrer);
        RefPtr<SecurityOrigin> referrerOrigin = SecurityOrigin::createFromString(referrer);
        addHTTPOriginIfNeeded(request, referrerOrigin->toString());
    }
#if ENABLE(CACHE_PARTITIONING)
    if (&m_frame.tree().top() != &m_frame)
        request.setCachePartition(m_frame.tree().top().document()->securityOrigin()->cachePartition());
#endif
    addExtraFieldsToRequest(request, newLoadType, true);
    if (newLoadType == FrameLoadTypeReload || newLoadType == FrameLoadTypeReloadFromOrigin)
        request.setCachePolicy(ReloadIgnoringCacheData);

    ASSERT(newLoadType != FrameLoadTypeSame);

    // The search for a target frame is done earlier in the case of form submission.
    Frame* targetFrame = isFormSubmission ? 0 : findFrameForNavigation(frameName);
    if (targetFrame && targetFrame != &m_frame) {
        targetFrame->loader().loadURL(newURL, referrer, "_self", lockHistory, newLoadType, event, formState.release());
        return;
    }

    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    NavigationAction action(request, newLoadType, isFormSubmission, event);

    if (!targetFrame && !frameName.isEmpty()) {
        policyChecker().checkNewWindowPolicy(action, request, formState.release(), frameName, [this](const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action, bool shouldContinue) {
            continueLoadAfterNewWindowPolicy(request, formState, frameName, action, shouldContinue);
        });
        return;
    }

    RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;

    bool sameURL = shouldTreatURLAsSameAsCurrent(newURL);
    const String& httpMethod = request.httpMethod();
    
    // Make sure to do scroll to fragment processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (shouldPerformFragmentNavigation(isFormSubmission, httpMethod, newLoadType, newURL)) {
        oldDocumentLoader->setTriggeringAction(action);
        oldDocumentLoader->setLastCheckedRequest(ResourceRequest());
        policyChecker().stopCheck();
        policyChecker().setLoadType(newLoadType);
        policyChecker().checkNavigationPolicy(request, oldDocumentLoader.get(), formState.release(), [this](const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue) {
            continueFragmentScrollAfterNavigationPolicy(request, shouldContinue);
        });
        return;
    }

    // must grab this now, since this load may stop the previous load and clear this flag
    bool isRedirect = m_quickRedirectComing;
    loadWithNavigationAction(request, action, lockHistory, newLoadType, formState.release());
    if (isRedirect) {
        m_quickRedirectComing = false;
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->setIsClientRedirect(true);
    } else if (sameURL && newLoadType != FrameLoadTypeReload && newLoadType != FrameLoadTypeReloadFromOrigin) {
        // Example of this case are sites that reload the same URL with a different cookie
        // driving the generated content, or a master frame with links that drive a target
        // frame, where the user has clicked on the same link repeatedly.
        m_loadType = FrameLoadTypeSame;
    }
}

SubstituteData FrameLoader::defaultSubstituteDataForURL(const URL& url)
{
    if (!shouldTreatURLAsSrcdocDocument(url))
        return SubstituteData();
    String srcdoc = m_frame.ownerElement()->fastGetAttribute(srcdocAttr);
    ASSERT(!srcdoc.isNull());
    CString encodedSrcdoc = srcdoc.utf8();
    return SubstituteData(SharedBuffer::create(encodedSrcdoc.data(), encodedSrcdoc.length()), "text/html", "UTF-8", URL());
}

void FrameLoader::load(const FrameLoadRequest& passedRequest)
{
    FrameLoadRequest request(passedRequest);

    if (m_inStopAllLoaders)
        return;

    if (!request.frameName().isEmpty()) {
        Frame* frame = findFrameForNavigation(request.frameName());
        if (frame) {
            request.setShouldCheckNewWindowPolicy(false);
            if (&frame->loader() != this) {
                frame->loader().load(request);
                return;
            }
        }
    }

    if (request.shouldCheckNewWindowPolicy()) {
        policyChecker().checkNewWindowPolicy(NavigationAction(request.resourceRequest(), NavigationTypeOther), request.resourceRequest(), nullptr, request.frameName(), [this](const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action, bool shouldContinue) {
            continueLoadAfterNewWindowPolicy(request, formState, frameName, action, shouldContinue);
        });

        return;
    }

    if (!request.hasSubstituteData())
        request.setSubstituteData(defaultSubstituteDataForURL(request.resourceRequest().url()));

    RefPtr<DocumentLoader> loader = m_client.createDocumentLoader(request.resourceRequest(), request.substituteData());
    load(loader.get());
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, const NavigationAction& action, LockHistory lockHistory, FrameLoadType type, PassRefPtr<FormState> formState)
{
    RefPtr<DocumentLoader> loader = m_client.createDocumentLoader(request, defaultSubstituteDataForURL(request.url()));
    if (lockHistory == LockHistory::Yes && m_documentLoader)
        loader->setClientRedirectSourceForHistory(m_documentLoader->didCreateGlobalHistoryEntry() ? m_documentLoader->urlForHistory().string() : m_documentLoader->clientRedirectSourceForHistory());

    loader->setTriggeringAction(action);
    if (m_documentLoader)
        loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    loadWithDocumentLoader(loader.get(), type, formState);
}

void FrameLoader::load(DocumentLoader* newDocumentLoader)
{
    ResourceRequest& r = newDocumentLoader->request();
    addExtraFieldsToMainResourceRequest(r);
    FrameLoadType type;

    if (shouldTreatURLAsSameAsCurrent(newDocumentLoader->originalRequest().url())) {
        r.setCachePolicy(ReloadIgnoringCacheData);
        type = FrameLoadTypeSame;
    } else if (shouldTreatURLAsSameAsCurrent(newDocumentLoader->unreachableURL()) && m_loadType == FrameLoadTypeReload)
        type = FrameLoadTypeReload;
    else
        type = FrameLoadTypeStandard;

    if (m_documentLoader)
        newDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the history list, we treat it as a reload so the history list 
    // is appropriately maintained.
    //
    // FIXME: This seems like a dangerous overloading of the meaning of "FrameLoadTypeReload" ...
    // shouldn't a more explicit type of reload be defined, that means roughly 
    // "load without affecting history" ? 
    if (shouldReloadToHandleUnreachableURL(newDocumentLoader)) {
        // shouldReloadToHandleUnreachableURL() returns true only when the original load type is back-forward.
        // In this case we should save the document state now. Otherwise the state can be lost because load type is
        // changed and updateForBackForwardNavigation() will not be called when loading is committed.
        history().saveDocumentAndScrollState();

        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }

    loadWithDocumentLoader(newDocumentLoader, type, 0);
}

void FrameLoader::loadWithDocumentLoader(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> prpFormState)
{
    // Retain because dispatchBeforeLoadEvent may release the last reference to it.
    Ref<Frame> protect(m_frame);

    ASSERT(m_client.hasWebView());

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT(m_frame.view());

    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    if (m_frame.document())
        m_previousURL = m_frame.document()->url();

    policyChecker().setLoadType(type);
    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;

    const URL& newURL = loader->request().url();
    const String& httpMethod = loader->request().httpMethod();

    if (shouldPerformFragmentNavigation(isFormSubmission, httpMethod, policyChecker().loadType(), newURL)) {
        RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;
        NavigationAction action(loader->request(), policyChecker().loadType(), isFormSubmission);

        oldDocumentLoader->setTriggeringAction(action);
        oldDocumentLoader->setLastCheckedRequest(ResourceRequest());
        policyChecker().stopCheck();
        policyChecker().checkNavigationPolicy(loader->request(), oldDocumentLoader.get(), formState, [this](const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue) {
            continueFragmentScrollAfterNavigationPolicy(request, shouldContinue);
        });
        return;
    }

    if (Frame* parent = m_frame.tree().parent())
        loader->setOverrideEncoding(parent->loader().documentLoader()->overrideEncoding());

    policyChecker().stopCheck();
    setPolicyDocumentLoader(loader);
    if (loader->triggeringAction().isEmpty())
        loader->setTriggeringAction(NavigationAction(loader->request(), policyChecker().loadType(), isFormSubmission));

    if (Element* ownerElement = m_frame.ownerElement()) {
        // We skip dispatching the beforeload event if we've already
        // committed a real document load because the event would leak
        // subsequent activity by the frame which the parent frame isn't
        // supposed to learn. For example, if the child frame navigated to
        // a new URL, the parent frame shouldn't learn the URL.
        if (!m_stateMachine.committedFirstRealDocumentLoad()
            && !ownerElement->dispatchBeforeLoadEvent(loader->request().url().string())) {
            continueLoadAfterNavigationPolicy(loader->request(), formState, false);
            return;
        }
    }

    policyChecker().checkNavigationPolicy(loader->request(), loader, formState, [this](const ResourceRequest& request, PassRefPtr<FormState> formState, bool shouldContinue) {
        continueLoadAfterNavigationPolicy(request, formState, shouldContinue);
    });
}

void FrameLoader::reportLocalLoadFailed(Frame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not allowed to load local resource: " + url);
}

const ResourceRequest& FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->originalRequest();
}

bool FrameLoader::willLoadMediaElementURL(URL& url)
{
#if PLATFORM(IOS)
    // MobileStore depends on the iOS 4.0 era client delegate method because webView:resource:willSendRequest:redirectResponse:fromDataSource
    // doesn't let them tell when a load request is coming from a media element. See <rdar://problem/8266916> for more details.
    if (applicationIsMobileStore())
        return m_client.shouldLoadMediaElementURL(url);
#endif

    ResourceRequest request(url);

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, request, ResourceResponse(url, String(), -1, String(), String()), 0, -1, -1, error);

    url = request.url();

    return error.isNull();
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(DocumentLoader* docLoader)
{
    URL unreachableURL = docLoader->unreachableURL();

    if (unreachableURL.isEmpty())
        return false;

    if (!isBackForwardLoadType(policyChecker().loadType()))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    DocumentLoader* compareDocumentLoader = 0;
    if (policyChecker().delegateIsDecidingNavigationPolicy() || policyChecker().delegateIsHandlingUnimplementablePolicy())
        compareDocumentLoader = m_policyDocumentLoader.get();
    else if (m_delegateIsHandlingProvisionalLoadError)
        compareDocumentLoader = m_provisionalDocumentLoader.get();

    return compareDocumentLoader && unreachableURL == compareDocumentLoader->request().url();
}

void FrameLoader::reloadWithOverrideEncoding(const String& encoding)
{
    if (!m_documentLoader)
        return;

    ResourceRequest request = m_documentLoader->request();
    URL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        request.setURL(unreachableURL);

    // FIXME: If the resource is a result of form submission and is not cached, the form will be silently resubmitted.
    // We should ask the user for confirmation in this case.
    request.setCachePolicy(ReturnCacheDataElseLoad);

    RefPtr<DocumentLoader> loader = m_client.createDocumentLoader(request, defaultSubstituteDataForURL(request.url()));
    setPolicyDocumentLoader(loader.get());

    loader->setOverrideEncoding(encoding);

    loadWithDocumentLoader(loader.get(), FrameLoadTypeReload, 0);
}

void FrameLoader::reload(bool endToEndReload)
{
    if (!m_documentLoader)
        return;

    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if (m_documentLoader->request().url().isEmpty())
        return;

    // Replace error-page URL with the URL we were trying to reach.
    ResourceRequest initialRequest = m_documentLoader->request();
    URL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        initialRequest.setURL(unreachableURL);

    // Create a new document loader for the reload, this will become m_documentLoader eventually,
    // but first it has to be the "policy" document loader, and then the "provisional" document loader.
    RefPtr<DocumentLoader> loader = m_client.createDocumentLoader(initialRequest, defaultSubstituteDataForURL(initialRequest.url()));

    ResourceRequest& request = loader->request();

    // FIXME: We don't have a mechanism to revalidate the main resource without reloading at the moment.
    request.setCachePolicy(ReloadIgnoringCacheData);

    // If we're about to re-post, set up action so the application can warn the user.
    if (request.httpMethod() == "POST")
        loader->setTriggeringAction(NavigationAction(request, NavigationTypeFormResubmitted));

    loader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    loadWithDocumentLoader(loader.get(), endToEndReload ? FrameLoadTypeReloadFromOrigin : FrameLoadTypeReload, 0);
}

void FrameLoader::stopAllLoaders(ClearProvisionalItemPolicy clearProvisionalItemPolicy)
{
    ASSERT(!m_frame.document() || !m_frame.document()->inPageCache());
    if (m_pageDismissalEventBeingDispatched != NoDismissal)
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;
    
    // Calling stopLoading() on the provisional document loader can blow away
    // the frame from underneath.
    Ref<Frame> protect(m_frame);

    m_inStopAllLoaders = true;

    policyChecker().stopCheck();

    // If no new load is in progress, we should clear the provisional item from history
    // before we call stopLoading.
    if (clearProvisionalItemPolicy == ShouldClearProvisionalItem)
        history().setProvisionalItem(0);

    for (RefPtr<Frame> child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->loader().stopAllLoaders(clearProvisionalItemPolicy);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    setProvisionalDocumentLoader(0);

    m_checkTimer.stop();

    m_inStopAllLoaders = false;    
}

void FrameLoader::stopForUserCancel(bool deferCheckLoadComplete)
{
    stopAllLoaders();

#if PLATFORM(IOS)
    // Lay out immediately when stopping to immediately clear the old page if we just committed this one
    // but haven't laid out/painted yet.
    // FIXME: Is this behavior specific to iOS? Or should we expose a setting to toggle this behavior?
    if (m_frame.view() && !m_frame.view()->didFirstLayout())
        m_frame.view()->layout();
#endif

    if (deferCheckLoadComplete)
        scheduleCheckLoadComplete();
    else if (m_frame.page())
        checkLoadComplete();
}

DocumentLoader* FrameLoader::activeDocumentLoader() const
{
    if (m_state == FrameStateProvisional)
        return m_provisionalDocumentLoader.get();
    return m_documentLoader.get();
}

bool FrameLoader::isLoading() const
{
    DocumentLoader* docLoader = activeDocumentLoader();
    if (!docLoader)
        return false;
    return docLoader->isLoading();
}

bool FrameLoader::frameHasLoaded() const
{
    return m_stateMachine.committedFirstRealDocumentLoad() || (m_provisionalDocumentLoader && !m_stateMachine.creatingInitialEmptyDocument()); 
}

void FrameLoader::setDocumentLoader(DocumentLoader* loader)
{
    if (!loader && !m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    m_client.prepareForDataSourceReplacement();
    detachChildren();

    // detachChildren() can trigger this frame's unload event, and therefore
    // script can run and do just about anything. For example, an unload event that calls
    // document.write("") on its parent frame can lead to a recursive detachChildren()
    // invocation for this frame. In that case, we can end up at this point with a
    // loader that hasn't been deleted but has been detached from its frame. Such a
    // DocumentLoader has been sufficiently detached that we'll end up in an inconsistent
    // state if we try to use it.
    if (loader && !loader->frame())
        return;

    if (m_documentLoader)
        m_documentLoader->detachFromFrame();

    m_documentLoader = loader;
}

void FrameLoader::setPolicyDocumentLoader(DocumentLoader* loader)
{
    if (m_policyDocumentLoader == loader)
        return;

    if (loader)
        loader->setFrame(&m_frame);
    if (m_policyDocumentLoader
            && m_policyDocumentLoader != m_provisionalDocumentLoader
            && m_policyDocumentLoader != m_documentLoader)
        m_policyDocumentLoader->detachFromFrame();

    m_policyDocumentLoader = loader;
}

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
}

void FrameLoader::setState(FrameState newState)
{    
    m_state = newState;
    
    if (newState == FrameStateProvisional)
        provisionalLoadStarted();
    else if (newState == FrameStateComplete) {
        frameLoadCompleted();
        if (m_documentLoader)
            m_documentLoader->stopRecordingResponses();
    }
}

void FrameLoader::clearProvisionalLoad()
{
    setProvisionalDocumentLoader(0);
    m_progressTracker->progressCompleted();
    setState(FrameStateComplete);
}

void FrameLoader::commitProvisionalLoad()
{
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    Ref<Frame> protect(m_frame);

    std::unique_ptr<CachedPage> cachedPage;
    if (m_loadingFromCachedPage)
        cachedPage = pageCache()->take(history().provisionalItem());

    LOG(PageCache, "WebCoreLoading %s: About to commit provisional load from previous URL '%s' to new URL '%s'", m_frame.tree().uniqueName().string().utf8().data(),
        m_frame.document() ? m_frame.document()->url().stringCenterEllipsizedToLength().utf8().data() : "",
        pdl ? pdl->url().stringCenterEllipsizedToLength().utf8().data() : "<no provisional DocumentLoader>");

#if PLATFORM(IOS)
    // In the case where we are not navigating to a cached page, and the system is under (speculative) memory pressure,
    // we can try to preemptively release some of the pages in the cache.
    // FIXME: Right now the capacity is 1 on iOS devices with 256 MB of RAM, so this will always blow away the whole
    // page cache. We could still preemptively prune the page cache while navigating to a cached page if capacity > 1.
    // See <rdar://problem/11779846> for more details.
    if (!cachedPage) {
        if (memoryPressureHandler().isUnderMemoryPressure()) {
            LOG(MemoryPressure, "Pruning page cache because under memory pressure at: %s", __PRETTY_FUNCTION__);
            LOG(PageCache, "Pruning page cache to 0 due to memory pressure");
            // Don't cache any page if we are under memory pressure.
            pageCache()->pruneToCapacityNow(0);
        } else if (systemMemoryLevel() <= memoryLevelThresholdToPrunePageCache) {
            LOG(MemoryPressure, "Pruning page cache because system memory level is %d at: %s", systemMemoryLevel(), __PRETTY_FUNCTION__);
            LOG(PageCache, "Pruning page cache to %d due to low memory (level %d less or equal to %d threshold)", pageCache()->capacity() / 2, systemMemoryLevel(), memoryLevelThresholdToPrunePageCache);
            pageCache()->pruneToCapacityNow(pageCache()->capacity() / 2);
        }
    }
#endif

    willTransitionToCommitted();

    // Check to see if we need to cache the page we are navigating away from into the back/forward cache.
    // We are doing this here because we know for sure that a new page is about to be loaded.
    HistoryItem* item = history().currentItem();
    if (!m_frame.tree().parent() && pageCache()->canCache(m_frame.page()) && !item->isInPageCache())
        pageCache()->add(item, *m_frame.page());

    if (m_loadType != FrameLoadTypeReplace)
        closeOldDataSources();

    if (!cachedPage && !m_stateMachine.creatingInitialEmptyDocument())
        m_client.makeRepresentation(pdl.get());

    transitionToCommitted(cachedPage.get());

    if (pdl && m_documentLoader) {
        // Check if the destination page is allowed to access the previous page's timing information.
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(pdl->request().url());
        m_documentLoader->timing()->setHasSameOriginAsPreviousDocument(securityOrigin->canRequest(m_previousURL));
    }

    // Call clientRedirectCancelledOrFinished() here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (m_sentRedirectNotification)
        clientRedirectCancelledOrFinished(false);
    
    if (cachedPage && cachedPage->document()) {
#if PLATFORM(IOS)
        // FIXME: CachedPage::restore() would dispatch viewport change notification. However UIKit expects load
        // commit to happen before any changes to viewport arguments and dealing with this there is difficult.
        m_frame.page()->chrome().setDispatchViewportDataDidChangeSuppressed(true);
#endif
        prepareForCachedPageRestore();

        // FIXME: This API should be turned around so that we ground CachedPage into the Page.
        cachedPage->restore(*m_frame.page());

        dispatchDidCommitLoad();
#if PLATFORM(IOS)
        m_frame.page()->chrome().setDispatchViewportDataDidChangeSuppressed(false);
        m_frame.page()->chrome().dispatchViewportPropertiesDidChange(m_frame.page()->viewportArguments());
#endif
        // If we have a title let the WebView know about it. 
        StringWithDirection title = m_documentLoader->title();
        if (!title.isNull())
            m_client.dispatchDidReceiveTitle(title);

        checkCompleted();
    } else
        didOpenURL();

    LOG(Loading, "WebCoreLoading %s: Finished committing provisional load to URL %s", m_frame.tree().uniqueName().string().utf8().data(),
        m_frame.document() ? m_frame.document()->url().stringCenterEllipsizedToLength().utf8().data() : "");

    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        history().updateForClientRedirect();

    if (m_loadingFromCachedPage) {
#if PLATFORM(IOS)
        // Note, didReceiveDocType is expected to be called for cached pages. See <rdar://problem/5906758> for more details.
        if (m_frame.document()->doctype() && m_frame.page())
            m_frame.page()->chrome().didReceiveDocType(&m_frame);
#endif
        m_frame.document()->documentDidResumeFromPageCache();

        // Force a layout to update view size and thereby update scrollbars.
#if PLATFORM(IOS)
        if (!m_client.forceLayoutOnRestoreFromPageCache())
            m_frame.view()->forceLayout();
#else
        m_frame.view()->forceLayout();
#endif

        const ResponseVector& responses = m_documentLoader->responses();
        size_t count = responses.size();
        for (size_t i = 0; i < count; i++) {
            const ResourceResponse& response = responses[i];
            // FIXME: If the WebKit client changes or cancels the request, this is not respected.
            ResourceError error;
            unsigned long identifier;
            ResourceRequest request(response.url());
            requestFromDelegate(request, identifier, error);
            // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
            // However, with today's computers and networking speeds, this won't happen in practice.
            // Could be an issue with a giant local file.
            notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, request, response, 0, static_cast<int>(response.expectedContentLength()), 0, error);
        }

        // FIXME: Why only this frame and not parent frames?
        checkLoadCompleteForThisFrame();
    }
}

void FrameLoader::transitionToCommitted(CachedPage* cachedPage)
{
    ASSERT(m_client.hasWebView());
    ASSERT(m_state == FrameStateProvisional);

    if (m_state != FrameStateProvisional)
        return;

    if (FrameView* view = m_frame.view()) {
        if (ScrollAnimator* scrollAnimator = view->existingScrollAnimator())
            scrollAnimator->cancelAnimations();
    }

    m_client.setCopiesOnScroll();
    history().updateForCommit();

    // The call to closeURL() invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    DocumentLoader* pdl = m_provisionalDocumentLoader.get();
    if (m_documentLoader)
        closeURL();
    if (pdl != m_provisionalDocumentLoader)
        return;

    // Nothing else can interupt this commit - set the Provisional->Committed transition in stone
    if (m_documentLoader)
        m_documentLoader->stopLoadingSubresources();
    if (m_documentLoader)
        m_documentLoader->stopLoadingPlugIns();

    setDocumentLoader(m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(0);

    if (pdl != m_documentLoader) {
        ASSERT(m_state == FrameStateComplete);
        return;
    }

    setState(FrameStateCommittedPage);

#if ENABLE(TOUCH_EVENTS) && !PLATFORM(IOS)
    if (m_frame.isMainFrame())
        m_frame.page()->chrome().client().needTouchEvents(false);
#endif

    // Handle adding the URL to the back/forward list.
    DocumentLoader* dl = m_documentLoader.get();

    switch (m_loadType) {
        case FrameLoadTypeForward:
        case FrameLoadTypeBack:
        case FrameLoadTypeIndexedBackForward:
            if (m_frame.page()) {
                // If the first load within a frame is a navigation within a back/forward list that was attached
                // without any of the items being loaded then we need to update the history in a similar manner as
                // for a standard load with the exception of updating the back/forward list (<rdar://problem/8091103>).
                if (!m_stateMachine.committedFirstRealDocumentLoad() && m_frame.isMainFrame())
                    history().updateForStandardLoad(HistoryController::UpdateAllExceptBackForwardList);

                history().updateForBackForwardNavigation();

                // For cached pages, CachedFrame::restore will take care of firing the popstate event with the history item's state object
                if (history().currentItem() && !cachedPage)
                    m_pendingStateObject = history().currentItem()->stateObject();

                // Create a document view for this document, or used the cached view.
                if (cachedPage) {
                    DocumentLoader* cachedDocumentLoader = cachedPage->documentLoader();
                    ASSERT(cachedDocumentLoader);
                    cachedDocumentLoader->setFrame(&m_frame);
                    m_client.transitionToCommittedFromCachedFrame(cachedPage->cachedMainFrame());
                } else
                    m_client.transitionToCommittedForNewPage();
            }
            break;

        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            history().updateForReload();
            m_client.transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeStandard:
            history().updateForStandardLoad();
            if (m_frame.view())
                m_frame.view()->setScrollbarsSuppressed(true);
            m_client.transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeRedirectWithLockedBackForwardList:
            history().updateForRedirectWithLockedBackForwardList();
            m_client.transitionToCommittedForNewPage();
            break;

        // FIXME Remove this check when dummy ds is removed (whatever "dummy ds" is).
        // An exception should be thrown if we're in the FrameLoadTypeUninitialized state.
        default:
            ASSERT_NOT_REACHED();
    }

    m_documentLoader->writer().setMIMEType(dl->responseMIMEType());

    // Tell the client we've committed this URL.
    ASSERT(m_frame.view());

    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
}

void FrameLoader::clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress)
{
    // Note that -webView:didCancelClientRedirectForFrame: is called on the frame load delegate even if
    // the redirect succeeded.  We should either rename this API, or add a new method, like
    // -webView:didFinishClientRedirectForFrame:
    m_client.dispatchDidCancelClientRedirect();

    if (!cancelWithLoadInProgress)
        m_quickRedirectComing = false;

    m_sentRedirectNotification = false;
}

void FrameLoader::clientRedirected(const URL& url, double seconds, double fireDate, LockBackForwardList lockBackForwardList)
{
    m_client.dispatchWillPerformClientRedirect(url, seconds, fireDate);
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    m_sentRedirectNotification = true;
    
    // If a "quick" redirect comes in, we set a special mode so we treat the next
    // load as part of the original navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    // Loads triggered by JavaScript form submissions never count as quick redirects.
    m_quickRedirectComing = (lockBackForwardList == LockBackForwardList::Yes || history().currentItemShouldBeReplaced()) && m_documentLoader && !m_isExecutingJavaScriptFormAction;
}

bool FrameLoader::shouldReload(const URL& currentURL, const URL& destinationURL)
{
    // This function implements the rule: "Don't reload if navigating by fragment within
    // the same URL, but do reload if going to a new URL or to the same URL with no
    // fragment identifier at all."
    if (!destinationURL.hasFragmentIdentifier())
        return true;
    return !equalIgnoringFragmentIdentifier(currentURL, destinationURL);
}

void FrameLoader::closeOldDataSources()
{
    // FIXME: Is it important for this traversal to be postorder instead of preorder?
    // If so, add helpers for postorder traversal, and use them. If not, then lets not
    // use a recursive algorithm here.
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->loader().closeOldDataSources();
    
    if (m_documentLoader)
        m_client.dispatchWillClose();

    m_client.setMainFrameDocumentReady(false); // stop giving out the actual DOMDocument to observers
}

void FrameLoader::prepareForCachedPageRestore()
{
    ASSERT(!m_frame.tree().parent());
    ASSERT(m_frame.page());
    ASSERT(m_frame.isMainFrame());

    m_frame.navigationScheduler().cancel();

    // We still have to close the previous part page.
    closeURL();
    
    // Delete old status bar messages (if it _was_ activated on last URL).
    if (m_frame.script().canExecuteScripts(NotAboutToExecuteScript)) {
        DOMWindow* window = m_frame.document()->domWindow();
        window->setStatus(String());
        window->setDefaultStatus(String());
    }
}

void FrameLoader::open(CachedFrameBase& cachedFrame)
{
    m_isComplete = false;
    
    // Don't re-emit the load event.
    m_didCallImplicitClose = true;

    URL url = cachedFrame.url();

    // FIXME: I suspect this block of code doesn't do anything.
    if (url.protocolIsInHTTPFamily() && !url.host().isEmpty() && url.path().isEmpty())
        url.setPath("/");

    started();
    Document* document = cachedFrame.document();
    ASSERT(document);
    ASSERT(document->domWindow());

    clear(document, true, true, cachedFrame.isMainFrame());

    document->setInPageCache(false);

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_outgoingReferrer = url.string();

    FrameView* view = cachedFrame.view();
    
    // When navigating to a CachedFrame its FrameView should never be null.  If it is we'll crash in creative ways downstream.
    ASSERT(view);
    view->setWasScrolledByUser(false);

    // Use the current ScrollView's frame rect.
    if (m_frame.view())
        view->setFrameRect(m_frame.view()->frameRect());
    m_frame.setView(view);
    
    m_frame.setDocument(document);
    document->domWindow()->resumeFromPageCache();

    updateFirstPartyForCookies();

    cachedFrame.restore();
}

bool FrameLoader::isHostedByObjectElement() const
{
    HTMLFrameOwnerElement* owner = m_frame.ownerElement();
    return owner && owner->hasTagName(objectTag);
}

bool FrameLoader::isReplacing() const
{
    return m_loadType == FrameLoadTypeReplace;
}

void FrameLoader::setReplacing()
{
    m_loadType = FrameLoadTypeReplace;
}

bool FrameLoader::subframeIsLoading() const
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (Frame* child = m_frame.tree().lastChild(); child; child = child->tree().previousSibling()) {
        FrameLoader& childLoader = child->loader();
        DocumentLoader* documentLoader = childLoader.documentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader.provisionalDocumentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader.policyDocumentLoader();
        if (documentLoader)
            return true;
    }
    return false;
}

void FrameLoader::willChangeTitle(DocumentLoader* loader)
{
    m_client.willChangeTitle(loader);
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}
    
CachePolicy FrameLoader::subresourceCachePolicy() const
{
    if (m_isComplete)
        return CachePolicyVerify;

    if (m_loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame.tree().parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader().subresourceCachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }
    
    if (m_loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    const ResourceRequest& request(documentLoader()->request());
#if PLATFORM(COCOA)
    if (request.cachePolicy() == ReloadIgnoringCacheData && !equalIgnoringCase(request.httpMethod(), "post") && ResourceRequest::useQuickLookResourceCachingQuirks())
        return CachePolicyRevalidate;
#endif

    if (request.cachePolicy() == ReturnCacheDataElseLoad)
        return CachePolicyHistoryBuffer;

    return CachePolicyVerify;
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client.hasWebView());

    switch (m_state) {
        case FrameStateProvisional: {
            if (m_delegateIsHandlingProvisionalLoadError)
                return;

            RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
            if (!pdl)
                return;
                
            // If we've received any errors we may be stuck in the provisional state and actually complete.
            const ResourceError& error = pdl->mainDocumentError();
            if (error.isNull())
                return;

            // Check all children first.
            RefPtr<HistoryItem> item;
            if (Page* page = m_frame.page())
                if (isBackForwardLoadType(loadType()))
                    // Reset the back forward list to the last committed history item at the top level.
                    item = page->mainFrame().loader().history().currentItem();
                
            // Only reset if we aren't already going to a new provisional item.
            bool shouldReset = !history().provisionalItem();
            if (!pdl->isLoadingInAPISense() || pdl->isStopping()) {
                m_delegateIsHandlingProvisionalLoadError = true;
                m_client.dispatchDidFailProvisionalLoad(error);
                m_delegateIsHandlingProvisionalLoadError = false;

                ASSERT(!pdl->isLoading());

                // If we're in the middle of loading multipart data, we need to restore the document loader.
                if (isReplacing() && !m_documentLoader.get())
                    setDocumentLoader(m_provisionalDocumentLoader.get());

                // Finish resetting the load state, but only if another load hasn't been started by the
                // delegate callback.
                if (pdl == m_provisionalDocumentLoader)
                    clearProvisionalLoad();
                else if (activeDocumentLoader()) {
                    URL unreachableURL = activeDocumentLoader()->unreachableURL();
                    if (!unreachableURL.isEmpty() && unreachableURL == pdl->request().url())
                        shouldReset = false;
                }
            }
            if (shouldReset && item)
                if (Page* page = m_frame.page()) {
                    page->backForward().setCurrentItem(item.get());
                    m_frame.loader().client().updateGlobalHistoryItemForPage();
                }
            return;
        }
        
        case FrameStateCommittedPage: {
            DocumentLoader* dl = m_documentLoader.get();            
            if (!dl || (dl->isLoadingInAPISense() && !dl->isStopping()))
                return;

            setState(FrameStateComplete);

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            m_client.forceLayoutForNonHTML();
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if (m_frame.page()) {
                if (isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin)
                    history().restoreScrollPositionAndViewState();
            }

            if (m_stateMachine.creatingInitialEmptyDocument() || !m_stateMachine.committedFirstRealDocumentLoad())
                return;

            m_progressTracker->progressCompleted();
            if (Page* page = m_frame.page()) {
                if (m_frame.isMainFrame())
                    page->resetRelevantPaintedObjectCounter();
            }

            const ResourceError& error = dl->mainDocumentError();

            AXObjectCache::AXLoadingEvent loadingEvent;
            if (!error.isNull()) {
                m_client.dispatchDidFailLoad(error);
                loadingEvent = AXObjectCache::AXLoadingFailed;
            } else {
                m_client.dispatchDidFinishLoad();
                loadingEvent = AXObjectCache::AXLoadingFinished;
            }

            // Notify accessibility.
            if (AXObjectCache* cache = m_frame.document()->existingAXObjectCache())
                cache->frameLoadingEventNotification(&m_frame, loadingEvent);

            return;
        }
        
        case FrameStateComplete:
            m_loadType = FrameLoadTypeStandard;
            frameLoadCompleted();
            return;
    }

    ASSERT_NOT_REACHED();
}

void FrameLoader::continueLoadAfterWillSubmitForm()
{
    if (!m_provisionalDocumentLoader)
        return;

    prepareForLoadStart();
    
    // The load might be cancelled inside of prepareForLoadStart(), nulling out the m_provisionalDocumentLoader, 
    // so we need to null check it again.
    if (!m_provisionalDocumentLoader)
        return;

    DocumentLoader* activeDocLoader = activeDocumentLoader();
    if (activeDocLoader && activeDocLoader->isLoadingMainResource())
        return;

    m_loadingFromCachedPage = false;
    m_provisionalDocumentLoader->startLoadingMainResource();
}

static URL originatingURLFromBackForwardList(Page* page)
{
    // FIXME: Can this logic be replaced with m_frame.document()->firstPartyForCookies()?
    // It has the same meaning of "page a user thinks is the current one".

    URL originalURL;
    int backCount = page->backForward().backCount();
    for (int backIndex = 0; backIndex <= backCount; backIndex++) {
        // FIXME: At one point we had code here to check a "was user gesture" flag.
        // Do we need to restore that logic?
        HistoryItem* historyItem = page->backForward().itemAtIndex(-backIndex);
        if (!historyItem)
            continue;

        originalURL = historyItem->originalURL(); 
        if (!originalURL.isNull()) 
            return originalURL;
    }

    return URL();
}

void FrameLoader::setOriginalURLForDownloadRequest(ResourceRequest& request)
{
    URL originalURL;
    
    // If there is no referrer, assume that the download was initiated directly, so current document is
    // completely unrelated to it. See <rdar://problem/5294691>.
    // FIXME: Referrer is not sent in many other cases, so we will often miss this important information.
    // Find a better way to decide whether the download was unrelated to current document.
    if (!request.httpReferrer().isNull()) {
        // find the first item in the history that was originated by the user
        originalURL = originatingURLFromBackForwardList(m_frame.page());
    }

    if (originalURL.isNull())
        originalURL = request.url();

    if (!originalURL.protocol().isEmpty() && !originalURL.host().isEmpty()) {
        unsigned port = originalURL.port();

        // Original URL is needed to show the user where a file was downloaded from. We should make a URL that won't result in downloading the file again.
        // FIXME: Using host-only URL is a very heavy-handed approach. We should attempt to provide the actual page where the download was initiated from, as a reminder to the user.
        String hostOnlyURLString;
        if (port)
            hostOnlyURLString = makeString(originalURL.protocol(), "://", originalURL.host(), ":", String::number(port));
        else
            hostOnlyURLString = makeString(originalURL.protocol(), "://", originalURL.host());

        // FIXME: Rename firstPartyForCookies back to mainDocumentURL. It was a mistake to think that it was only used for cookies.
        request.setFirstPartyForCookies(URL(URL(), hostOnlyURLString));
    }
}

void FrameLoader::didLayout(LayoutMilestones milestones)
{
    ASSERT(m_frame.isMainFrame());

    m_client.dispatchDidLayout(milestones);
}

void FrameLoader::didFirstLayout()
{
#if PLATFORM(IOS)
    // Only send layout-related delegate callbacks synchronously for the main frame to
    // avoid reentering layout for the main frame while delivering a layout-related delegate
    // callback for a subframe.
    if (&m_frame != &m_frame.page()->mainFrame())
        return;
#endif
    if (m_frame.page() && isBackForwardLoadType(m_loadType))
        history().restoreScrollPositionAndViewState();

    if (m_stateMachine.committedFirstRealDocumentLoad() && !m_stateMachine.isDisplayingInitialEmptyDocument() && !m_stateMachine.firstLayoutDone())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::FirstLayoutDone);
}

void FrameLoader::frameLoadCompleted()
{
    // Note: Can be called multiple times.

    m_client.frameLoadCompleted();

    history().updateForFrameLoadCompleted();

    // After a canceled provisional load, firstLayoutDone is false.
    // Reset it to true if we're displaying a page.
    if (m_documentLoader && m_stateMachine.committedFirstRealDocumentLoad() && !m_stateMachine.isDisplayingInitialEmptyDocument() && !m_stateMachine.firstLayoutDone())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::FirstLayoutDone);
}

void FrameLoader::detachChildren()
{
    Vector<Ref<Frame>, 16> childrenToDetach;
    childrenToDetach.reserveInitialCapacity(m_frame.tree().childCount());
    for (Frame* child = m_frame.tree().lastChild(); child; child = child->tree().previousSibling())
        childrenToDetach.uncheckedAppend(*child);
    for (unsigned i = 0; i < childrenToDetach.size(); ++i)
        childrenToDetach[i]->loader().detachFromParent();
}

void FrameLoader::closeAndRemoveChild(Frame* child)
{
    child->tree().detachFromParent();

    child->setView(0);
    if (child->ownerElement() && child->page())
        child->page()->decrementSubframeCount();
    child->willDetachPage();
    child->detachFromPage();

    m_frame.tree().removeChild(child);
}

// Called every time a resource is completely loaded or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(m_client.hasWebView());
    
    m_shouldCallCheckLoadComplete = false;

    if (!m_frame.page())
        return;

    // FIXME: Always traversing the entire frame tree is a bit inefficient, but 
    // is currently needed in order to null out the previous history item for all frames.
    Vector<Ref<Frame>, 16> frames;
    for (Frame* frame = &m_frame.mainFrame(); frame; frame = frame->tree().traverseNext())
        frames.append(*frame);

    // To process children before their parents, iterate the vector backwards.
    for (unsigned i = frames.size(); i; --i)
        frames[i - 1]->loader().checkLoadCompleteForThisFrame();
}

int FrameLoader::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return m_frame.document()->cachedResourceLoader()->requestCount();

    int count = 0;
    for (Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext(&m_frame))
        count += frame->document()->cachedResourceLoader()->requestCount();
    return count;
}

String FrameLoader::userAgent(const URL& url) const
{
    return m_client.userAgent(url);
}

void FrameLoader::handledOnloadEvents()
{
    m_client.dispatchDidHandleOnloadEvents();

    if (documentLoader())
        documentLoader()->handledOnloadEvents();
}

void FrameLoader::frameDetached()
{
    stopAllLoaders();
    m_frame.document()->stopActiveDOMObjects();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    Ref<Frame> protect(m_frame);

    closeURL();
    history().saveScrollPositionAndViewStateToItem(history().currentItem());
    detachChildren();
    // stopAllLoaders() needs to be called after detachChildren(), because detachedChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    stopAllLoaders();

    InspectorInstrumentation::frameDetachedFromParent(&m_frame);

    detachViewsAndDocumentLoader();

    m_progressTracker = nullptr;

    if (Frame* parent = m_frame.tree().parent()) {
        parent->loader().closeAndRemoveChild(&m_frame);
        parent->loader().scheduleCheckCompleted();
    } else {
        m_frame.setView(0);
        m_frame.willDetachPage();
        m_frame.detachFromPage();
    }
}

void FrameLoader::detachViewsAndDocumentLoader()
{
    m_client.detachedFromParent2();
    setDocumentLoader(0);
    m_client.detachedFromParent3();
}
    
void FrameLoader::addExtraFieldsToSubresourceRequest(ResourceRequest& request)
{
    addExtraFieldsToRequest(request, m_loadType, false);
}

void FrameLoader::addExtraFieldsToMainResourceRequest(ResourceRequest& request)
{
    // FIXME: Using m_loadType seems wrong for some callers.
    // If we are only preparing to load the main resource, that is previous load's load type!
    addExtraFieldsToRequest(request, m_loadType, true);
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request, FrameLoadType loadType, bool mainResource)
{
    // Don't set the cookie policy URL if it's already been set.
    // But make sure to set it on all requests regardless of protocol, as it has significance beyond the cookie policy (<rdar://problem/6616664>).
    if (request.firstPartyForCookies().isEmpty()) {
        if (mainResource && m_frame.isMainFrame())
            request.setFirstPartyForCookies(request.url());
        else if (Document* document = m_frame.document())
            request.setFirstPartyForCookies(document->firstPartyForCookies());
    }

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    applyUserAgent(request);

    if (!mainResource) {
        if (request.isConditional())
            request.setCachePolicy(ReloadIgnoringCacheData);
        else if (documentLoader()->isLoadingInAPISense()) {
            // If we inherit cache policy from a main resource, we use the DocumentLoader's
            // original request cache policy for two reasons:
            // 1. For POST requests, we mutate the cache policy for the main resource,
            //    but we do not want this to apply to subresources
            // 2. Delegates that modify the cache policy using willSendRequest: should
            //    not affect any other resources. Such changes need to be done
            //    per request.
            ResourceRequestCachePolicy mainDocumentOriginalCachePolicy = documentLoader()->originalRequest().cachePolicy();
            // Back-forward navigations try to load main resource from cache only to avoid re-submitting form data, and start over (with a warning dialog) if that fails.
            // This policy is set on initial request too, but should not be inherited.
            ResourceRequestCachePolicy subresourceCachePolicy = (mainDocumentOriginalCachePolicy == ReturnCacheDataDontLoad) ? ReturnCacheDataElseLoad : mainDocumentOriginalCachePolicy;
            request.setCachePolicy(subresourceCachePolicy);
        } else
            request.setCachePolicy(UseProtocolCachePolicy);

    // FIXME: Other FrameLoader functions have duplicated code for setting cache policy of main request when reloading.
    // It seems better to manage it explicitly than to hide the logic inside addExtraFieldsToRequest().
    } else if (loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadFromOrigin || request.isConditional())
        request.setCachePolicy(ReloadIgnoringCacheData);

    if (request.cachePolicy() == ReloadIgnoringCacheData) {
        if (loadType == FrameLoadTypeReload)
            request.setHTTPHeaderField("Cache-Control", "max-age=0");
        else if (loadType == FrameLoadTypeReloadFromOrigin) {
            request.setHTTPHeaderField("Cache-Control", "no-cache");
            request.setHTTPHeaderField("Pragma", "no-cache");
        }
    }
    
    if (mainResource)
        request.setHTTPAccept(defaultAcceptHeader);

    // Make sure we send the Origin header.
    addHTTPOriginIfNeeded(request, String());

    // Only set fallback array if it's still empty (later attempts may be incorrect, see bug 117818).
    if (request.responseContentDispositionEncodingFallbackArray().isEmpty()) {
        // Always try UTF-8. If that fails, try frame encoding (if any) and then the default.
        request.setResponseContentDispositionEncodingFallbackArray("UTF-8", m_frame.document()->encoding(), m_frame.settings().defaultTextEncodingName());
    }
}

void FrameLoader::addHTTPOriginIfNeeded(ResourceRequest& request, const String& origin)
{
    if (!request.httpOrigin().isEmpty())
        return;  // Request already has an Origin header.

    // Don't send an Origin header for GET or HEAD to avoid privacy issues.
    // For example, if an intranet page has a hyperlink to an external web
    // site, we don't want to include the Origin of the request because it
    // will leak the internal host name. Similar privacy concerns have lead
    // to the widespread suppression of the Referer header at the network
    // layer.
    if (request.httpMethod() == "GET" || request.httpMethod() == "HEAD")
        return;

    // For non-GET and non-HEAD methods, always send an Origin header so the
    // server knows we support this feature.

    if (origin.isEmpty()) {
        // If we don't know what origin header to attach, we attach the value
        // for an empty origin.
        request.setHTTPOrigin(SecurityOrigin::createUnique()->toString());
        return;
    }

    request.setHTTPOrigin(origin);
}

void FrameLoader::loadPostRequest(const ResourceRequest& inRequest, const String& referrer, const String& frameName, LockHistory lockHistory, FrameLoadType loadType, PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
{
    RefPtr<FormState> formState = prpFormState;

    // Previously when this method was reached, the original FrameLoadRequest had been deconstructed to build a 
    // bunch of parameters that would come in here and then be built back up to a ResourceRequest.  In case
    // any caller depends on the immutability of the original ResourceRequest, I'm rebuilding a ResourceRequest
    // from scratch as it did all along.
    const URL& url = inRequest.url();
    RefPtr<FormData> formData = inRequest.httpBody();
    const String& contentType = inRequest.httpContentType();
    String origin = inRequest.httpOrigin();

    ResourceRequest workingResourceRequest(url);    

    if (!referrer.isEmpty())
        workingResourceRequest.setHTTPReferrer(referrer);
    workingResourceRequest.setHTTPOrigin(origin);
    workingResourceRequest.setHTTPMethod("POST");
    workingResourceRequest.setHTTPBody(formData);
    workingResourceRequest.setHTTPContentType(contentType);
    addExtraFieldsToRequest(workingResourceRequest, loadType, true);

    NavigationAction action(workingResourceRequest, loadType, true, event);

    if (!frameName.isEmpty()) {
        // The search for a target frame is done earlier in the case of form submission.
        if (Frame* targetFrame = formState ? 0 : findFrameForNavigation(frameName)) {
            targetFrame->loader().loadWithNavigationAction(workingResourceRequest, action, lockHistory, loadType, formState.release());
            return;
        }

        policyChecker().checkNewWindowPolicy(action, workingResourceRequest, formState.release(), frameName, [this](const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action, bool shouldContinue) {
            continueLoadAfterNewWindowPolicy(request, formState, frameName, action, shouldContinue);
        });
        return;
    }

    // must grab this now, since this load may stop the previous load and clear this flag
    bool isRedirect = m_quickRedirectComing;
    loadWithNavigationAction(workingResourceRequest, action, lockHistory, loadType, formState.release());    
    if (isRedirect) {
        m_quickRedirectComing = false;
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->setIsClientRedirect(true);
    }
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ClientCredentialPolicy clientCredentialPolicy, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    ASSERT(m_frame.document());
    String referrer = SecurityPolicy::generateReferrerHeader(m_frame.document()->referrerPolicy(), request.url(), outgoingReferrer());
    
    ResourceRequest initialRequest = request;
    initialRequest.setTimeoutInterval(10);
    
    if (!referrer.isEmpty())
        initialRequest.setHTTPReferrer(referrer);
    addHTTPOriginIfNeeded(initialRequest, outgoingOrigin());

    initialRequest.setFirstPartyForCookies(m_frame.mainFrame().loader().documentLoader()->request().url());
    
    addExtraFieldsToSubresourceRequest(initialRequest);

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        
        if (!documentLoader()->applicationCacheHost()->maybeLoadSynchronously(newRequest, error, response, data)) {
            platformStrategies()->loaderStrategy()->loadResourceSynchronously(networkingContext(), identifier, newRequest, storedCredentials, clientCredentialPolicy, error, response, data);
            documentLoader()->applicationCacheHost()->maybeLoadFallbackSynchronously(newRequest, error, response, data);
        }
    }
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, request, response, data.data(), data.size(), -1, error);
    return identifier;
}

const ResourceRequest& FrameLoader::originalRequest() const
{
    return activeDocumentLoader()->originalRequestCopy();
}

void FrameLoader::receivedMainResourceError(const ResourceError& error)
{
    // Retain because the stop may release the last reference to it.
    Ref<Frame> protect(m_frame);

    RefPtr<DocumentLoader> loader = activeDocumentLoader();
    // FIXME: Don't want to do this if an entirely new load is going, so should check
    // that both data sources on the frame are either this or nil.
    stop();
    if (m_client.shouldFallBack(error))
        handleFallbackContent();

    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        if (m_submittedFormURL == m_provisionalDocumentLoader->originalRequestCopy().url())
            m_submittedFormURL = URL();
            
        // We might have made a page cache item, but now we're bailing out due to an error before we ever
        // transitioned to the new page (before WebFrameState == commit).  The goal here is to restore any state
        // so that the existing view (that wenever got far enough to replace) can continue being used.
        history().invalidateCurrentItemCachedPage();
        
        // Call clientRedirectCancelledOrFinished here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect. The frame load delegate may have saved some state about
        // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:. Since we are definitely
        // not going to use this provisional resource, as it was cancelled, notify the frame load delegate that the redirect
        // has ended.
        if (m_sentRedirectNotification)
            clientRedirectCancelledOrFinished(false);
    }

    checkCompleted();
    if (m_frame.page())
        checkLoadComplete();
}

void FrameLoader::continueFragmentScrollAfterNavigationPolicy(const ResourceRequest& request, bool shouldContinue)
{
    m_quickRedirectComing = false;

    if (!shouldContinue)
        return;

    // If we have a provisional request for a different document, a fragment scroll should cancel it.
    if (m_provisionalDocumentLoader && !equalIgnoringFragmentIdentifier(m_provisionalDocumentLoader->request().url(), request.url())) {
        m_provisionalDocumentLoader->stopLoading();
        setProvisionalDocumentLoader(0);
    }

    bool isRedirect = m_quickRedirectComing || policyChecker().loadType() == FrameLoadTypeRedirectWithLockedBackForwardList;    
    loadInSameDocument(request.url(), 0, !isRedirect);
}

bool FrameLoader::shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType loadType, const URL& url)
{
    // We don't do this if we are submitting a form with method other than "GET", explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    // These rules were originally based on what KHTML was doing in KHTMLPart::openURL.

    // FIXME: What about load types other than Standard and Reload?

    return (!isFormSubmission || equalIgnoringCase(httpMethod, "GET"))
        && loadType != FrameLoadTypeReload
        && loadType != FrameLoadTypeReloadFromOrigin
        && loadType != FrameLoadTypeSame
        && !shouldReload(m_frame.document()->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame.document()->isFrameSet();
}

void FrameLoader::scrollToFragmentWithParentBoundary(const URL& url)
{
    FrameView* view = m_frame.view();
    if (!view)
        return;

    // Leaking scroll position to a cross-origin ancestor would permit the so-called "framesniffing" attack.
    RefPtr<Frame> boundaryFrame(url.hasFragmentIdentifier() ? m_frame.document()->findUnsafeParentScrollPropagationBoundary() : 0);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(false);

    view->scrollToFragment(url);

    if (boundaryFrame)
        boundaryFrame->view()->setSafeToPropagateScrollToParent(true);
}

bool FrameLoader::shouldClose()
{
    Page* page = m_frame.page();
    if (!page)
        return true;
    if (!page->chrome().canRunBeforeUnloadConfirmPanel())
        return true;

    // Store all references to each subframe in advance since beforeunload's event handler may modify frame
    Vector<Ref<Frame>, 16> targetFrames;
    targetFrames.append(m_frame);
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().traverseNext(&m_frame))
        targetFrames.append(*child);

    bool shouldClose = false;
    {
        NavigationDisablerForBeforeUnload navigationDisabler;
        size_t i;

        for (i = 0; i < targetFrames.size(); i++) {
            if (!targetFrames[i]->tree().isDescendantOf(&m_frame))
                continue;
            if (!targetFrames[i]->loader().handleBeforeUnloadEvent(page->chrome(), this))
                break;
        }

        if (i == targetFrames.size())
            shouldClose = true;
    }

    if (!shouldClose)
        m_submittedFormURL = URL();

    m_currentNavigationHasShownBeforeUnloadConfirmPanel = false;
    return shouldClose;
}

bool FrameLoader::handleBeforeUnloadEvent(Chrome& chrome, FrameLoader* frameLoaderBeingNavigated)
{
    DOMWindow* domWindow = m_frame.document()->domWindow();
    if (!domWindow)
        return true;

    RefPtr<Document> document = m_frame.document();
    if (!document->body())
        return true;
    
    RefPtr<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();
    m_pageDismissalEventBeingDispatched = BeforeUnloadDismissal;

    // We store the frame's page in a local variable because the frame might get detached inside dispatchEvent.
    Page* page = m_frame.page();
    page->incrementFrameHandlingBeforeUnloadEventCount();
    domWindow->dispatchEvent(beforeUnloadEvent.get(), domWindow->document());
    page->decrementFrameHandlingBeforeUnloadEventCount();

    m_pageDismissalEventBeingDispatched = NoDismissal;

    if (!beforeUnloadEvent->defaultPrevented())
        document->defaultEventHandler(beforeUnloadEvent.get());
    if (beforeUnloadEvent->returnValue().isNull())
        return true;

    // If the navigating FrameLoader has already shown a beforeunload confirmation panel for the current navigation attempt,
    // this frame is not allowed to cause another one to be shown.
    if (frameLoaderBeingNavigated->m_currentNavigationHasShownBeforeUnloadConfirmPanel) {
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Blocked attempt to show multiple beforeunload confirmation dialogs for the same navigation."));
        return true;
    }

    // We should only display the beforeunload dialog for an iframe if its SecurityOrigin matches all
    // ancestor frame SecurityOrigins up through the navigating FrameLoader.
    if (frameLoaderBeingNavigated != this) {
        Frame* parentFrame = m_frame.tree().parent();
        while (parentFrame) {
            Document* parentDocument = parentFrame->document();
            if (!parentDocument)
                return true;
            if (!m_frame.document() || !m_frame.document()->securityOrigin()->canAccess(parentDocument->securityOrigin())) {
                document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, ASCIILiteral("Blocked attempt to show beforeunload confirmation dialog on behalf of a frame with different security origin. Protocols, domains, and ports must match."));
                return true;
            }
            
            if (&parentFrame->loader() == frameLoaderBeingNavigated)
                break;
            
            parentFrame = parentFrame->tree().parent();
        }
        
        // The navigatingFrameLoader should always be in our ancestory.
        ASSERT(parentFrame);
        ASSERT(&parentFrame->loader() == frameLoaderBeingNavigated);
    }

    frameLoaderBeingNavigated->m_currentNavigationHasShownBeforeUnloadConfirmPanel = true;

    String text = document->displayStringModifiedByEncoding(beforeUnloadEvent->returnValue());
    return chrome.runBeforeUnloadConfirmPanel(text, &m_frame);
}

void FrameLoader::continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState> formState, bool shouldContinue)
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT(m_policyDocumentLoader || !m_provisionalDocumentLoader->unreachableURL().isEmpty());

    bool isTargetItem = history().provisionalItem() ? history().provisionalItem()->isTargetItem() : false;

    // Two reasons we can't continue:
    //    1) Navigation policy delegate said we can't so request is nil. A primary case of this 
    //       is the user responding Cancel to the form repost nag sheet.
    //    2) User responded Cancel to an alert popped up by the before unload event handler.
    bool canContinue = shouldContinue && shouldClose();

    if (!canContinue) {
        // If we were waiting for a quick redirect, but the policy delegate decided to ignore it, then we 
        // need to report that the client redirect was cancelled.
        if (m_quickRedirectComing)
            clientRedirectCancelledOrFinished(false);

        setPolicyDocumentLoader(0);

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || m_frame.isMainFrame()) && isBackForwardLoadType(policyChecker().loadType())) {
            if (Page* page = m_frame.page()) {
                if (HistoryItem* resetItem = m_frame.mainFrame().loader().history().currentItem()) {
                    page->backForward().setCurrentItem(resetItem);
                    m_frame.loader().client().updateGlobalHistoryItemForPage();
                }
            }
        }
        return;
    }

    FrameLoadType type = policyChecker().loadType();
    // A new navigation is in progress, so don't clear the history's provisional item.
    stopAllLoaders(ShouldNotClearProvisionalItem);
    
    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load. 
    if (!m_frame.page())
        return;

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame.page()) {
        if (m_frame.isMainFrame())
            page->inspectorController().resume();
    }
#endif

    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_loadType = type;
    setState(FrameStateProvisional);

    setPolicyDocumentLoader(0);

    if (isBackForwardLoadType(type) && history().provisionalItem()->isInPageCache()) {
        loadProvisionalItemFromCachedPage();
        return;
    }

    if (!formState) {
        continueLoadAfterWillSubmitForm();
        return;
    }

    m_client.dispatchWillSubmitForm(formState, [this](PolicyAction action) {
        policyChecker().continueLoadAfterWillSubmitForm(action);
    });
}

void FrameLoader::continueLoadAfterNewWindowPolicy(const ResourceRequest& request,
    PassRefPtr<FormState> formState, const String& frameName, const NavigationAction& action, bool shouldContinue)
{
    if (!shouldContinue)
        return;

    Ref<Frame> frame(m_frame);
    RefPtr<Frame> mainFrame = m_client.dispatchCreatePage(action);
    if (!mainFrame)
        return;

    if (frameName != "_blank")
        mainFrame->tree().setName(frameName);

    mainFrame->page()->setOpenedByDOM();
    mainFrame->loader().m_client.dispatchShow();
    if (!m_suppressOpenerInNewFrame) {
        mainFrame->loader().setOpener(&frame.get());
        mainFrame->document()->setReferrerPolicy(frame->document()->referrerPolicy());
    }
    mainFrame->loader().loadWithNavigationAction(request, NavigationAction(request), LockHistory::No, FrameLoadTypeStandard, formState);
}

void FrameLoader::requestFromDelegate(ResourceRequest& request, unsigned long& identifier, ResourceError& error)
{
    ASSERT(!request.isNull());

    identifier = 0;
    if (Page* page = m_frame.page()) {
        identifier = page->progress().createUniqueIdentifier();
        notifier().assignIdentifierToInitialRequest(identifier, m_documentLoader.get(), request);
    }

    ResourceRequest newRequest(request);
    notifier().dispatchWillSendRequest(m_documentLoader.get(), identifier, newRequest, ResourceResponse());

    if (newRequest.isNull())
        error = cancelledError(request);
    else
        error = ResourceError();

    request = newRequest;
}

void FrameLoader::loadedResourceFromMemoryCache(CachedResource* resource, ResourceRequest& newRequest)
{
    Page* page = m_frame.page();
    if (!page)
        return;

    if (!resource->shouldSendResourceLoadCallbacks() || m_documentLoader->haveToldClientAboutLoad(resource->url()))
        return;

    // Main resource delegate messages are synthesized in MainResourceLoader, so we must not send them here.
    if (resource->type() == CachedResource::MainResource)
        return;

    if (!page->areMemoryCacheClientCallsEnabled()) {
        InspectorInstrumentation::didLoadResourceFromMemoryCache(page, m_documentLoader.get(), resource);
        m_documentLoader->recordMemoryCacheLoadForFutureClientNotification(resource->resourceRequest());
        m_documentLoader->didTellClientAboutLoad(resource->url());
        return;
    }

    if (m_client.dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), newRequest, resource->response(), resource->encodedSize())) {
        InspectorInstrumentation::didLoadResourceFromMemoryCache(page, m_documentLoader.get(), resource);
        m_documentLoader->didTellClientAboutLoad(resource->url());
        return;
    }

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(newRequest, identifier, error);
    InspectorInstrumentation::markResourceAsCached(page, identifier);
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, newRequest, resource->response(), 0, resource->encodedSize(), 0, error);
}

void FrameLoader::applyUserAgent(ResourceRequest& request)
{
    String userAgent = this->userAgent(request.url());
    ASSERT(!userAgent.isNull());
    request.setHTTPUserAgent(userAgent);
}

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const URL& url, unsigned long requestIdentifier)
{
    Frame& topFrame = m_frame.tree().top();
    if (&m_frame == &topFrame)
        return false;

    XFrameOptionsDisposition disposition = parseXFrameOptionsHeader(content);

    switch (disposition) {
    case XFrameOptionsSameOrigin: {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
        if (!origin->isSameSchemeHostPort(topFrame.document()->securityOrigin()))
            return true;
        for (Frame* frame = m_frame.tree().parent(); frame; frame = frame->tree().parent()) {
            if (!origin->isSameSchemeHostPort(frame->document()->securityOrigin()))
                break;
        }
        return false;
    }
    case XFrameOptionsDeny:
        return true;
    case XFrameOptionsAllowAll:
        return false;
    case XFrameOptionsConflict:
        m_frame.document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Multiple 'X-Frame-Options' headers with conflicting values ('" + content + "') encountered when loading '" + url.stringCenterEllipsizedToLength() + "'. Falling back to 'DENY'.", requestIdentifier);
        return true;
    case XFrameOptionsInvalid:
        m_frame.document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Invalid 'X-Frame-Options' header encountered when loading '" + url.stringCenterEllipsizedToLength() + "': '" + content + "' is not a recognized directive. The header will be ignored.", requestIdentifier);
        return false;
    case XFrameOptionsNone:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void FrameLoader::loadProvisionalItemFromCachedPage()
{
    DocumentLoader* provisionalLoader = provisionalDocumentLoader();
    LOG(PageCache, "WebCorePageCache: Loading provisional DocumentLoader %p with URL '%s' from CachedPage", provisionalDocumentLoader(), provisionalDocumentLoader()->url().stringCenterEllipsizedToLength().utf8().data());

    prepareForLoadStart();

    m_loadingFromCachedPage = true;

    // Should have timing data from previous time(s) the page was shown.
    ASSERT(provisionalLoader->timing()->navigationStart());
    provisionalLoader->resetTiming();
    provisionalLoader->timing()->markNavigationStart();

    provisionalLoader->setCommitted(true);
    commitProvisionalLoad();
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const URL& url) const
{
    if (!history().currentItem())
        return false;
    return url == history().currentItem()->url() || url == history().currentItem()->originalURL();
}

bool FrameLoader::shouldTreatURLAsSrcdocDocument(const URL& url) const
{
    if (!equalIgnoringCase(url.string(), "about:srcdoc"))
        return false;
    HTMLFrameOwnerElement* ownerElement = m_frame.ownerElement();
    if (!ownerElement)
        return false;
    if (!ownerElement->hasTagName(iframeTag))
        return false;
    return ownerElement->fastHasAttribute(srcdocAttr);
}

Frame* FrameLoader::findFrameForNavigation(const AtomicString& name, Document* activeDocument)
{
    Frame* frame = m_frame.tree().find(name);

    // FIXME: Eventually all callers should supply the actual activeDocument so we can call canNavigate with the right document.
    if (!activeDocument)
        activeDocument = m_frame.document();

    if (!activeDocument->canNavigate(frame))
        return 0;

    return frame;
}

void FrameLoader::loadSameDocumentItem(HistoryItem* item)
{
    ASSERT(item->documentSequenceNumber() == history().currentItem()->documentSequenceNumber());

    // Save user view state to the current history item here since we don't do a normal load.
    // FIXME: Does form state need to be saved here too?
    history().saveScrollPositionAndViewStateToItem(history().currentItem());
    if (FrameView* view = m_frame.view())
        view->setWasScrolledByUser(false);

    history().setCurrentItem(item);
        
    // loadInSameDocument() actually changes the URL and notifies load delegates of a "fake" load
    loadInSameDocument(item->url(), item->stateObject(), false);

    // Restore user view state from the current history item here since we don't do a normal load.
    history().restoreScrollPositionAndViewState();
}

// FIXME: This function should really be split into a couple pieces, some of
// which should be methods of HistoryController and some of which should be
// methods of FrameLoader.
void FrameLoader::loadDifferentDocumentItem(HistoryItem* item, FrameLoadType loadType, FormSubmissionCacheLoadPolicy cacheLoadPolicy)
{
    // Remember this item so we can traverse any child items as child frames load
    history().setProvisionalItem(item);

    if (CachedPage* cachedPage = pageCache()->get(item)) {
        loadWithDocumentLoader(cachedPage->documentLoader(), loadType, 0);   
        return;
    }

    URL itemURL = item->url();
    URL itemOriginalURL = item->originalURL();
    URL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->url();
    RefPtr<FormData> formData = item->formData();

    ResourceRequest request(itemURL);

    if (!item->referrer().isNull())
        request.setHTTPReferrer(item->referrer());
    
    // If this was a repost that failed the page cache, we might try to repost the form.
    NavigationAction action;
    if (formData) {
        formData->generateFiles(m_frame.document());

        request.setHTTPMethod("POST");
        request.setHTTPBody(formData);
        request.setHTTPContentType(item->formContentType());
        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromString(item->referrer());
        addHTTPOriginIfNeeded(request, securityOrigin->toString());

        // Make sure to add extra fields to the request after the Origin header is added for the FormData case.
        // See https://bugs.webkit.org/show_bug.cgi?id=22194 for more discussion.
        addExtraFieldsToRequest(request, loadType, true);
        
        // FIXME: Slight hack to test if the NSURL cache contains the page we're going to.
        // We want to know this before talking to the policy delegate, since it affects whether 
        // we show the DoYouReallyWantToRepost nag.
        //
        // This trick has a small bug (3123893) where we might find a cache hit, but then
        // have the item vanish when we try to use it in the ensuing nav.  This should be
        // extremely rare, but in that case the user will get an error on the navigation.
        
        if (cacheLoadPolicy == MayAttemptCacheOnlyLoadForFormSubmissionItem) {
            request.setCachePolicy(ReturnCacheDataDontLoad);
            action = NavigationAction(request, loadType, false);
        } else {
            request.setCachePolicy(ReturnCacheDataElseLoad);
            action = NavigationAction(request, NavigationTypeFormResubmitted);
        }
    } else {
        switch (loadType) {
            case FrameLoadTypeReload:
            case FrameLoadTypeReloadFromOrigin:
                request.setCachePolicy(ReloadIgnoringCacheData);
                break;
            case FrameLoadTypeBack:
            case FrameLoadTypeForward:
            case FrameLoadTypeIndexedBackForward:
                // If the first load within a frame is a navigation within a back/forward list that was attached 
                // without any of the items being loaded then we should use the default caching policy (<rdar://problem/8131355>).
                if (m_stateMachine.committedFirstRealDocumentLoad())
                    request.setCachePolicy(ReturnCacheDataElseLoad);
                break;
            case FrameLoadTypeStandard:
            case FrameLoadTypeRedirectWithLockedBackForwardList:
                break;
            case FrameLoadTypeSame:
            default:
                ASSERT_NOT_REACHED();
        }

        addExtraFieldsToRequest(request, loadType, true);

        ResourceRequest requestForOriginalURL(request);
        requestForOriginalURL.setURL(itemOriginalURL);
        action = NavigationAction(requestForOriginalURL, loadType, false);
    }

    loadWithNavigationAction(request, action, LockHistory::No, loadType, 0);
}

// Loads content into this frame, as specified by history item
void FrameLoader::loadItem(HistoryItem* item, FrameLoadType loadType)
{
    m_requestedHistoryItem = item;
    HistoryItem* currentItem = history().currentItem();
    bool sameDocumentNavigation = currentItem && item->shouldDoSameDocumentNavigationTo(currentItem);

    if (sameDocumentNavigation)
        loadSameDocumentItem(item);
    else
        loadDifferentDocumentItem(item, loadType, MayAttemptCacheOnlyLoadForFormSubmissionItem);
}

void FrameLoader::retryAfterFailedCacheOnlyMainResourceLoad()
{
    ASSERT(m_state == FrameStateProvisional);
    ASSERT(!m_loadingFromCachedPage);
    // We only use cache-only loads to avoid resubmitting forms.
    ASSERT(isBackForwardLoadType(m_loadType));
    ASSERT(history().provisionalItem()->formData());
    ASSERT(history().provisionalItem() == m_requestedHistoryItem.get());

    FrameLoadType loadType = m_loadType;
    HistoryItem* item = history().provisionalItem();

    stopAllLoaders(ShouldNotClearProvisionalItem);
    loadDifferentDocumentItem(item, loadType, MayNotAttemptCacheOnlyLoadForFormSubmissionItem);
}

ResourceError FrameLoader::cancelledError(const ResourceRequest& request) const
{
    ResourceError error = m_client.cancelledError(request);
    error.setIsCancellation(true);
    return error;
}

#if PLATFORM(IOS)
RetainPtr<CFDictionaryRef> FrameLoader::connectionProperties(ResourceLoader* loader)
{
    return m_client.connectionProperties(loader->documentLoader(), loader->identifier());
}
#endif

String FrameLoader::referrer() const
{
    return m_documentLoader ? m_documentLoader->request().httpReferrer() : "";
}

void FrameLoader::dispatchDidClearWindowObjectsInAllWorlds()
{
    if (!m_frame.script().canExecuteScripts(NotAboutToExecuteScript))
        return;

    Vector<Ref<DOMWrapperWorld>> worlds;
    ScriptController::getAllWorlds(worlds);
    for (size_t i = 0; i < worlds.size(); ++i)
        dispatchDidClearWindowObjectInWorld(worlds[i].get());
}

void FrameLoader::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    if (!m_frame.script().canExecuteScripts(NotAboutToExecuteScript) || !m_frame.script().existingWindowShell(world))
        return;

    m_client.dispatchDidClearWindowObjectInWorld(world);

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame.page())
        page->inspectorController().didClearWindowObjectInWorld(&m_frame, world);
#endif

    InspectorInstrumentation::didClearWindowObjectInWorld(&m_frame, world);
}

void FrameLoader::dispatchGlobalObjectAvailableInAllWorlds()
{
    Vector<Ref<DOMWrapperWorld>> worlds;
    ScriptController::getAllWorlds(worlds);
    for (size_t i = 0; i < worlds.size(); ++i)
        m_client.dispatchGlobalObjectAvailable(worlds[i].get());
}

SandboxFlags FrameLoader::effectiveSandboxFlags() const
{
    SandboxFlags flags = m_forcedSandboxFlags;
    if (Frame* parentFrame = m_frame.tree().parent())
        flags |= parentFrame->document()->sandboxFlags();
    if (HTMLFrameOwnerElement* ownerElement = m_frame.ownerElement())
        flags |= ownerElement->sandboxFlags();
    return flags;
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    m_client.didChangeTitle(loader);

    if (loader == m_documentLoader) {
        // Must update the entries in the back-forward list too.
        history().setCurrentItemTitle(loader->title());
        // This must go through the WebFrame because it has the right notion of the current b/f item.
        m_client.setTitle(loader->title(), loader->urlForHistory());
        m_client.setMainFrameDocumentReady(true); // update observers with new DOMDocument
        m_client.dispatchDidReceiveTitle(loader->title());
    }

#if ENABLE(REMOTE_INSPECTOR)
    if (m_frame.isMainFrame())
        m_frame.page()->remoteInspectorInformationDidChange();
#endif
}

void FrameLoader::didChangeIcons(IconType type)
{
    m_client.dispatchDidChangeIcons(type);
}

void FrameLoader::dispatchDidCommitLoad()
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    m_client.dispatchDidCommitLoad();

    if (m_frame.isMainFrame()) {
        m_frame.page()->resetSeenPlugins();
        m_frame.page()->resetSeenMediaEngines();
    }

    InspectorInstrumentation::didCommitLoad(&m_frame, m_documentLoader.get());

#if ENABLE(REMOTE_INSPECTOR)
    if (m_frame.isMainFrame())
        m_frame.page()->remoteInspectorInformationDidChange();
#endif
}

void FrameLoader::tellClientAboutPastMemoryCacheLoads()
{
    ASSERT(m_frame.page());
    ASSERT(m_frame.page()->areMemoryCacheClientCallsEnabled());

    if (!m_documentLoader)
        return;

    Vector<ResourceRequest> pastLoads;
    m_documentLoader->takeMemoryCacheLoadsForClientNotification(pastLoads);

    size_t size = pastLoads.size();
    for (size_t i = 0; i < size; ++i) {
        CachedResource* resource = memoryCache()->resourceForRequest(pastLoads[i], m_frame.page()->sessionID());

        // FIXME: These loads, loaded from cache, but now gone from the cache by the time
        // Page::setMemoryCacheClientCallsEnabled(true) is called, will not be seen by the client.
        // Consider if there's some efficient way of remembering enough to deliver this client call.
        // We have the URL, but not the rest of the response or the length.
        if (!resource)
            continue;

        ResourceRequest request(resource->url());
        m_client.dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize());
    }
}

NetworkingContext* FrameLoader::networkingContext() const
{
    return m_networkingContext.get();
}

void FrameLoader::loadProgressingStatusChanged()
{
    FrameView* view = m_frame.mainFrame().view();
    if (!view)
        return;

    view->updateLayerFlushThrottlingInAllFrames();
    view->adjustTiledBackingCoverage();
}

void FrameLoader::forcePageTransitionIfNeeded()
{
    m_client.forcePageTransitionIfNeeded();
}

bool FrameLoaderClient::hasHTMLView() const
{
    return true;
}

PassRefPtr<Frame> createWindow(Frame* openerFrame, Frame* lookupFrame, const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank") {
        if (Frame* frame = lookupFrame->loader().findFrameForNavigation(request.frameName(), openerFrame->document())) {
            if (request.frameName() != "_self") {
                if (Page* page = frame->page())
                    page->chrome().focus();
            }
            created = false;
            return frame;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (isDocumentSandboxed(openerFrame, SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked opening '" + request.resourceRequest().url().stringCenterEllipsizedToLength() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set.");
        return 0;
    }

    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    String referrer = SecurityPolicy::generateReferrerHeader(openerFrame->document()->referrerPolicy(), request.resourceRequest().url(), openerFrame->loader().outgoingReferrer());
    if (!referrer.isEmpty())
        requestWithReferrer.resourceRequest().setHTTPReferrer(referrer);
    FrameLoader::addHTTPOriginIfNeeded(requestWithReferrer.resourceRequest(), openerFrame->loader().outgoingOrigin());

    Page* oldPage = openerFrame->page();
    if (!oldPage)
        return 0;

    NavigationAction action(requestWithReferrer.resourceRequest());
    Page* page = oldPage->chrome().createWindow(openerFrame, requestWithReferrer, features, action);
    if (!page)
        return 0;

    page->mainFrame().loader().forceSandboxFlags(openerFrame->document()->sandboxFlags());

    if (request.frameName() != "_blank")
        page->mainFrame().tree().setName(request.frameName());

    page->chrome().setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    page->chrome().setStatusbarVisible(features.statusBarVisible);
    page->chrome().setScrollbarsVisible(features.scrollbarsVisible);
    page->chrome().setMenubarVisible(features.menuBarVisible);
    page->chrome().setResizable(features.resizable);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

// FIXME: We should reconcile the initialization of viewport arguments between iOS and OpenSource.
#if !PLATFORM(IOS)
    FloatSize viewportSize = page->chrome().pageRect().size();
    FloatRect windowRect = page->chrome().windowRect();
    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    // Zero width and height mean using default size, not minumum one.
    if (features.widthSet && features.width)
        windowRect.setWidth(features.width + (windowRect.width() - viewportSize.width()));
    if (features.heightSet && features.height)
        windowRect.setHeight(features.height + (windowRect.height() - viewportSize.height()));

    // Ensure non-NaN values, minimum size as well as being within valid screen area.
    FloatRect newWindowRect = DOMWindow::adjustWindowRect(page, windowRect);

    page->chrome().setWindowRect(newWindowRect);
#else
    // On iOS, width and height refer to the viewport dimensions.
    ViewportArguments arguments;
    // Zero width and height mean using default size, not minimum one.
    if (features.widthSet && features.width)
        arguments.width = features.width;
    if (features.heightSet && features.height)
        arguments.height = features.height;
    page->mainFrame().setViewportArguments(arguments);
#endif

    page->chrome().show();

    created = true;
    return &page->mainFrame();
}

} // namespace WebCore
