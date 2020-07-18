/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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
#include "BackForwardCache.h"
#include "BackForwardController.h"
#include "BeforeUnloadEvent.h"
#include "CachedPage.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "ContentFilter.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "DOMWindow.h"
#include "DatabaseManager.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
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
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClient.h"
#include "FrameNetworkingContext.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GCController.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "HTTPHeaderNames.h"
#include "HTTPHeaderValues.h"
#include "HTTPParsers.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "IgnoreOpensDuringUnloadCountIncrementer.h"
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "LinkLoader.h"
#include "LoadTiming.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "MemoryRelease.h"
#include "NavigationDisabler.h"
#include "NavigationScheduler.h"
#include "Node.h"
#include "Page.h"
#include "PageTransitionEvent.h"
#include "PerformanceLogging.h"
#include "PlatformStrategies.h"
#include "PluginData.h"
#include "PluginDocument.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "ResourceLoadInfo.h"
#include "ResourceLoadObserver.h"
#include "ResourceRequest.h"
#include "SVGDocument.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScrollAnimator.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "SegmentedString.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "ShouldTreatAsContinuingLoad.h"
#include "StyleTreeResolver.h"
#include "SubframeLoader.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include "UserContentController.h"
#include "UserGestureIndicator.h"
#include "WindowFeatures.h"
#include "XMLDocumentParser.h"
#include <dom/ScriptDisallowedScope.h>
#include <wtf/CompletionHandler.h>
#include <wtf/URL.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "Archive.h"
#endif

#if ENABLE(DATA_DETECTION)
#include "DataDetection.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "DocumentType.h"
#include "ResourceLoader.h"
#include "RuntimeApplicationChecks.h"
#endif

#define PAGE_ID ((pageID().valueOr(PageIdentifier())).toUInt64())
#define FRAME_ID ((frameID().valueOr(FrameIdentifier())).toUInt64())
#define FRAMELOADER_RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] FrameLoader::" fmt, this, PAGE_ID, FRAME_ID, m_frame.isMainFrame(), ##__VA_ARGS__)
#define FRAMELOADER_RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] FrameLoader::" fmt, this, PAGE_ID, FRAME_ID, m_frame.isMainFrame(), ##__VA_ARGS__)

namespace WebCore {

using namespace HTMLNames;
using namespace SVGNames;

bool isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
    case FrameLoadType::Standard:
    case FrameLoadType::Reload:
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::ReloadExpiredOnly:
    case FrameLoadType::Same:
    case FrameLoadType::RedirectWithLockedBackForwardList:
    case FrameLoadType::Replace:
        return false;
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool isReload(FrameLoadType type)
{
    switch (type) {
    case FrameLoadType::Reload:
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::ReloadExpiredOnly:
        return true;
    case FrameLoadType::Standard:
    case FrameLoadType::Same:
    case FrameLoadType::RedirectWithLockedBackForwardList:
    case FrameLoadType::Replace:
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
        return false;
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
static bool isDocumentSandboxed(Frame& frame, SandboxFlags mask)
{
    return frame.document() && frame.document()->isSandboxed(mask);
}

class PageLevelForbidScope {
protected:
    explicit PageLevelForbidScope(Page* page)
        : m_page(makeWeakPtr(page))
    {
    }

    ~PageLevelForbidScope() = default;

    WeakPtr<Page> m_page;
};

struct ForbidPromptsScope : public PageLevelForbidScope {
    explicit ForbidPromptsScope(Page* page)
        : PageLevelForbidScope(page)
    {
        if (m_page)
            m_page->forbidPrompts();
    }

    ~ForbidPromptsScope()
    {
        if (m_page)
            m_page->allowPrompts();
    }
};

struct ForbidSynchronousLoadsScope : public PageLevelForbidScope {
    explicit ForbidSynchronousLoadsScope(Page* page)
        : PageLevelForbidScope(page)
    {
        if (m_page)
            m_page->forbidSynchronousLoads();
    }

    ~ForbidSynchronousLoadsScope()
    {
        if (m_page)
            m_page->allowSynchronousLoads();
    }
};

class FrameLoader::FrameProgressTracker {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FrameProgressTracker(Frame& frame)
        : m_frame(frame)
        , m_inProgress(false)
    {
    }

    ~FrameProgressTracker()
    {
        if (m_inProgress && m_frame.page())
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
        platformStrategies()->loaderStrategy()->pageLoadCompleted(*m_frame.page());
    }

private:
    Frame& m_frame;
    bool m_inProgress;
};

FrameLoader::FrameLoader(Frame& frame, UniqueRef<FrameLoaderClient>&& client)
    : m_frame(frame)
    , m_client(WTFMove(client))
    , m_policyChecker(makeUnique<PolicyChecker>(frame))
    , m_history(makeUnique<HistoryController>(frame))
    , m_notifier(frame)
    , m_subframeLoader(makeUnique<SubframeLoader>(frame))
    , m_mixedContentChecker(frame)
    , m_state(FrameStateProvisional)
    , m_loadType(FrameLoadType::Standard)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_inStopAllLoaders(false)
    , m_isExecutingJavaScriptFormAction(false)
    , m_didCallImplicitClose(true)
    , m_wasUnloadEventEmitted(false)
    , m_isComplete(false)
    , m_needsClear(false)
    , m_checkTimer(*this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_shouldCallCheckLoadComplete(false)
    , m_opener(nullptr)
    , m_loadingFromCachedPage(false)
    , m_currentNavigationHasShownBeforeUnloadConfirmPanel(false)
    , m_loadsSynchronously(false)
    , m_forcedSandboxFlags(SandboxNone)
{
}

FrameLoader::~FrameLoader()
{
    setOpener(nullptr);
    detachFromAllOpenedFrames();

    if (m_networkingContext)
        m_networkingContext->invalidate();
}

void FrameLoader::detachFromAllOpenedFrames()
{
    for (auto& frame : m_openedFrames)
        frame->loader().m_opener = nullptr;
    m_openedFrames.clear();
}

void FrameLoader::init()
{
    // This somewhat odd set of steps gives the frame an initial empty document.
    setPolicyDocumentLoader(m_client->createDocumentLoader(ResourceRequest(URL({ }, emptyString())), SubstituteData()).ptr());
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_provisionalDocumentLoader->startLoadingMainResource();

    Ref<Frame> protect(m_frame);
    m_frame.document()->cancelParsing();
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);

    m_networkingContext = m_client->createNetworkingContext();
    m_progressTracker = makeUnique<FrameProgressTracker>(m_frame);
}

void FrameLoader::initForSynthesizedDocument(const URL&)
{
    // FIXME: We need to initialize the document URL to the specified URL. Currently the URL is empty and hence
    // FrameLoader::checkCompleted() will overwrite the URL of the document to be activeDocumentLoader()->documentURL().

    auto loader = m_client->createDocumentLoader(ResourceRequest(URL({ }, emptyString())), SubstituteData());
    loader->attachToFrame(m_frame);
    loader->setResponse(ResourceResponse(URL(), "text/html"_s, 0, String()));
    loader->setCommitted(true);
    setDocumentLoader(loader.ptr());

    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocument);
    m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
    m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    m_client->transitionToCommittedForNewPage();

    m_didCallImplicitClose = true;
    m_isComplete = true;
    m_state = FrameStateComplete;
    m_needsClear = true;

    m_networkingContext = m_client->createNetworkingContext();
    m_progressTracker = makeUnique<FrameProgressTracker>(m_frame);
}

Optional<PageIdentifier> FrameLoader::pageID() const
{
    return client().pageID();
}

Optional<FrameIdentifier> FrameLoader::frameID() const
{
    return client().frameID();
}

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

void FrameLoader::checkContentPolicy(const ResourceResponse& response, PolicyCheckIdentifier identifier, ContentPolicyDecisionFunction&& function)
{
    if (!activeDocumentLoader()) {
        // Load was cancelled
        function(PolicyAction::Ignore, identifier);
        return;
    }

    // FIXME: Validate the policy check identifier.
    client().dispatchDecidePolicyForResponse(response, activeDocumentLoader()->request(), identifier, activeDocumentLoader()->downloadAttribute(), WTFMove(function));
}

void FrameLoader::changeLocation(const URL& url, const String& passedTarget, Event* triggeringEvent, LockHistory lockHistory, LockBackForwardList lockBackForwardList, const ReferrerPolicy& referrerPolicy, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, Optional<NewFrameOpenerPolicy> openerPolicy, const AtomString& downloadAttribute, const SystemPreviewInfo& systemPreviewInfo, Optional<AdClickAttribution>&& adClickAttribution)
{
    auto* frame = lexicalFrameFromCommonVM();
    auto initiatedByMainFrame = frame && frame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;

    NewFrameOpenerPolicy newFrameOpenerPolicy = openerPolicy.valueOr(referrerPolicy == ReferrerPolicy::NoReferrer ? NewFrameOpenerPolicy::Suppress : NewFrameOpenerPolicy::Allow);
    FrameLoadRequest frameLoadRequest(*m_frame.document(), m_frame.document()->securityOrigin(), { url }, passedTarget, initiatedByMainFrame, downloadAttribute, systemPreviewInfo);
    frameLoadRequest.setLockHistory(lockHistory);
    frameLoadRequest.setLockBackForwardList(lockBackForwardList);
    frameLoadRequest.setNewFrameOpenerPolicy(newFrameOpenerPolicy);
    frameLoadRequest.setReferrerPolicy(referrerPolicy);
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.disableShouldReplaceDocumentIfJavaScriptURL();
    changeLocation(WTFMove(frameLoadRequest), triggeringEvent, WTFMove(adClickAttribution));
}

void FrameLoader::changeLocation(FrameLoadRequest&& frameRequest, Event* triggeringEvent, Optional<AdClickAttribution>&& adClickAttribution)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "changeLocation: frame load started");
    ASSERT(frameRequest.resourceRequest().httpMethod() == "GET");

    Ref<Frame> protect(m_frame);

    if (frameRequest.resourceRequest().url().protocolIsJavaScript()) {
        m_frame.script().executeJavaScriptURL(frameRequest.resourceRequest().url(), &frameRequest.requester().securityOrigin(), frameRequest.shouldReplaceDocumentIfJavaScriptURL());
        m_quickRedirectComing = false;
        return;
    }

    if (frameRequest.frameName().isEmpty())
        frameRequest.setFrameName(m_frame.document()->baseTarget());

    m_frame.document()->contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(frameRequest.resourceRequest(), ContentSecurityPolicy::InsecureRequestType::Navigation);

    loadFrameRequest(WTFMove(frameRequest), triggeringEvent, { }, WTFMove(adClickAttribution));
}

void FrameLoader::submitForm(Ref<FormSubmission>&& submission)
{
    ASSERT(submission->method() == FormSubmission::Method::Post || submission->method() == FormSubmission::Method::Get);

    // FIXME: Find a good spot for these.
    ASSERT(!submission->state().sourceDocument().frame() || submission->state().sourceDocument().frame() == &m_frame);

    if (!m_frame.page())
        return;

    if (submission->action().isEmpty())
        return;

    if (isDocumentSandboxed(m_frame, SandboxForms)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        m_frame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked form submission to '" + submission->action().stringCenterEllipsizedToLength() + "' because the form's frame is sandboxed and the 'allow-forms' permission is not set.");
        return;
    }

    URL formAction = submission->action();
    if (!m_frame.document()->contentSecurityPolicy()->allowFormAction(formAction))
        return;

    if (formAction.protocolIsJavaScript()) {
        m_isExecutingJavaScriptFormAction = true;
        Ref<Frame> protect(m_frame);
        m_frame.script().executeJavaScriptURL(submission->action(), nullptr, DoNotReplaceDocumentIfJavaScriptURL);
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    Frame* targetFrame = findFrameForNavigation(submission->target(), &submission->state().sourceDocument());
    if (!targetFrame) {
        if (!DOMWindow::allowPopUp(m_frame) && !UserGestureIndicator::processingUserGesture())
            return;

        // FIXME: targetFrame can be null for two distinct reasons:
        // 1. The frame was not found by name, so we should try opening a new window.
        // 2. The frame was found, but navigating it was not allowed, e.g. by HTML5 sandbox or by origin checks.
        // Continuing form submission makes no sense in the latter case.
        // There is a repeat check after timer fires, so this is not a correctness issue.

        targetFrame = &m_frame;
    } else
        submission->clearTarget();

    if (!targetFrame->page())
        return;

    if (m_frame.tree().isDescendantOf(targetFrame))
        m_submittedFormURL = submission->requestURL();

    submission->setReferrer(outgoingReferrer());
    submission->setOrigin(SecurityPolicy::generateOriginHeader(m_frame.document()->referrerPolicy(), submission->requestURL(), m_frame.document()->securityOrigin()));

    targetFrame->navigationScheduler().scheduleFormSubmission(WTFMove(submission));
}

void FrameLoader::stopLoading(UnloadEventPolicy unloadEventPolicy)
{
    if (m_frame.document() && m_frame.document()->parser())
        m_frame.document()->parser()->stopParsing();

    if (unloadEventPolicy != UnloadEventPolicyNone)
        dispatchUnloadEvents(unloadEventPolicy);

    m_isComplete = true; // to avoid calling completed() in finishedParsing()
    m_didCallImplicitClose = true; // don't want that one either

    if (m_frame.document() && m_frame.document()->parsing()) {
        finishedParsing();
        m_frame.document()->setParsing(false);
    }

    if (auto* document = m_frame.document()) {
        // FIXME: Should the DatabaseManager watch for something like ActiveDOMObject::stop() rather than being special-cased here?
        DatabaseManager::singleton().stopDatabases(*document, nullptr);
    }

    policyChecker().stopCheck();

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
}

void FrameLoader::willTransitionToCommitted()
{
    // This function is called when a frame is still fully in place (not cached, not detached), but will be replaced.
    Document* document = m_frame.document();
    if (!document)
        return;

    if (document->editor().hasComposition()) {
        // The text was already present in DOM, so it's better to confirm than to cancel the composition.
        document->editor().confirmComposition();
        if (EditorClient* editorClient = document->editor().client()) {
            editorClient->respondToChangedSelection(&m_frame);
            editorClient->discardedComposition(&m_frame);
        }
    }
}

void FrameLoader::closeURL()
{
    history().saveDocumentState();

    RefPtr<Document> currentDocument = m_frame.document();
    UnloadEventPolicy unloadEventPolicy;
    if (m_frame.page() && m_frame.page()->chrome().client().isSVGImageChromeClient()) {
        // If this is the SVGDocument of an SVGImage, no need to dispatch events or recalcStyle.
        unloadEventPolicy = UnloadEventPolicyNone;
    } else {
        // Should only send the pagehide event here if the current document exists and has not been placed in the back/forward cache.
        unloadEventPolicy = currentDocument && currentDocument->backForwardCacheState() == Document::NotInBackForwardCache ? UnloadEventPolicyUnloadAndPageHide : UnloadEventPolicyUnloadOnly;
    }

    stopLoading(unloadEventPolicy);
    
    if (currentDocument)
        currentDocument->editor().clearUndoRedoOperations();
}

bool FrameLoader::didOpenURL()
{
    if (m_frame.navigationScheduler().redirectScheduledDuringLoad()) {
        // A redirect was scheduled before the document was created.
        // This can happen when one frame changes another frame's location.
        return false;
    }

    m_frame.navigationScheduler().cancel();

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

    if (auto* document = m_frame.document())
        m_client->dispatchDidExplicitOpen(document->url(), document->contentType());
    
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

static inline bool shouldClearWindowName(const Frame& frame, const Document& newDocument)
{
    if (!frame.isMainFrame())
        return false;

    if (frame.loader().opener())
        return false;

    return !newDocument.securityOrigin().isSameOriginAs(frame.document()->securityOrigin());
}

void FrameLoader::clear(Document* newDocument, bool clearWindowProperties, bool clearScriptObjects, bool clearFrameView, WTF::Function<void()>&& handleDOMWindowCreation)
{
    bool neededClear = m_needsClear;
    m_needsClear = false;

    if (neededClear && m_frame.document()->backForwardCacheState() != Document::InBackForwardCache) {
        m_frame.document()->cancelParsing();
        m_frame.document()->stopActiveDOMObjects();
        bool hadLivingRenderTree = m_frame.document()->hasLivingRenderTree();
        m_frame.document()->willBeRemovedFromFrame();
        if (hadLivingRenderTree)
            m_frame.document()->adjustFocusedNodeOnNodeRemoval(*m_frame.document());
    }

    if (handleDOMWindowCreation)
        handleDOMWindowCreation();

    if (!neededClear)
        return;
    
    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        InspectorInstrumentation::frameWindowDiscarded(m_frame, m_frame.document()->domWindow());
        m_frame.document()->domWindow()->resetUnlessSuspendedForDocumentSuspension();
        m_frame.windowProxy().clearJSWindowProxiesNotMatchingDOMWindow(newDocument->domWindow(), m_frame.document()->backForwardCacheState() == Document::AboutToEnterBackForwardCache);

        if (shouldClearWindowName(m_frame, *newDocument))
            m_frame.tree().setName(nullAtom());
    }

    m_frame.eventHandler().clear();

    if (clearFrameView && m_frame.view())
        m_frame.view()->clear();

    // Do not drop the document before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame.setDocument(nullptr);

    subframeLoader().clear();

    if (clearWindowProperties)
        m_frame.windowProxy().setDOMWindow(newDocument->domWindow());

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
    auto protectedFrame = makeRef(m_frame);
    
    dispatchDidCommitLoad(WTF::nullopt, WTF::nullopt);
    dispatchDidClearWindowObjectsInAllWorlds();
    dispatchGlobalObjectAvailableInAllWorlds();

    if (!m_documentLoader)
        return;

    auto& documentLoader = *m_documentLoader;
    auto& title = documentLoader.title();
    if (!title.string.isNull())
        m_client->dispatchDidReceiveTitle(title);

    ASSERT(m_frame.document());
    auto& document = *m_frame.document();

    LinkLoader::loadLinksFromHeader(documentLoader.response().httpHeaderField(HTTPHeaderName::Link), document.url(), document, LinkLoader::MediaAttributeCheck::MediaAttributeEmpty);

    double delay;
    String urlString;
    if (!parseMetaHTTPEquivRefresh(documentLoader.response().httpHeaderField(HTTPHeaderName::Refresh), delay, urlString))
        return;
    auto completedURL = urlString.isEmpty() ? document.url() : document.completeURL(urlString);
    if (!completedURL.protocolIsJavaScript())
        m_frame.navigationScheduler().scheduleRedirect(document, delay, completedURL);
    else {
        auto message = "Refused to refresh " + document.url().stringCenterEllipsizedToLength() + " to a javascript: URL";
        document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
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
        m_frame.document()->statePopped(*m_pendingStateObject);
        m_pendingStateObject = nullptr;
    }

    if (dispatch)
        dispatchDidClearWindowObjectsInAllWorlds();

    updateFirstPartyForCookies();
    m_frame.document()->initContentSecurityPolicy();

    const Settings& settings = m_frame.settings();
    m_frame.document()->cachedResourceLoader().setImagesEnabled(settings.areImagesEnabled());
    m_frame.document()->cachedResourceLoader().setAutoLoadImages(settings.loadsImagesAutomatically());

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField(HTTPHeaderName::XDNSPrefetchControl);
        if (!dnsPrefetchControl.isEmpty())
            m_frame.document()->parseDNSPrefetchControlHeader(dnsPrefetchControl);

        m_frame.document()->contentSecurityPolicy()->didReceiveHeaders(ContentSecurityPolicyResponseHeaders(m_documentLoader->response()), referrer(), ContentSecurityPolicy::ReportParsingErrors::No);

        String referrerPolicy = m_documentLoader->response().httpHeaderField(HTTPHeaderName::ReferrerPolicy);
        if (!referrerPolicy.isNull())
            m_frame.document()->processReferrerPolicy(referrerPolicy, ReferrerPolicySource::HTTPHeader);

        String headerContentLanguage = m_documentLoader->response().httpHeaderField(HTTPHeaderName::ContentLanguage);
        if (!headerContentLanguage.isEmpty()) {
            size_t commaIndex = headerContentLanguage.find(',');
            headerContentLanguage.truncate(commaIndex); // notFound == -1 == don't truncate
            headerContentLanguage = stripLeadingAndTrailingHTMLSpaces(headerContentLanguage);
            if (!headerContentLanguage.isEmpty())
                m_frame.document()->setContentLanguage(headerContentLanguage);
        }
    }

    history().restoreDocumentState();
}

void FrameLoader::finishedParsing()
{
    LOG(Loading, "WebCoreLoading %s: Finished parsing", m_frame.tree().uniqueName().string().utf8().data());

    m_frame.injectUserScripts(UserScriptInjectionTime::DocumentEnd);

    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<Frame> protector = m_frame.view() ? &m_frame : 0;

    m_client->dispatchDidFinishDocumentLoad();

    scrollToFragmentWithParentBoundary(m_frame.document()->url());

    checkCompleted();

    if (!m_frame.view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame.view()->restoreScrollbar();
}

void FrameLoader::loadDone(LoadCompletionType type)
{
    if (type == LoadCompletionType::Finish)
        checkCompleted();
    else
        scheduleCheckCompleted();
}

void FrameLoader::subresourceLoadDone(LoadCompletionType type)
{
    if (type == LoadCompletionType::Finish)
        checkLoadComplete();
    else
        scheduleCheckLoadComplete();
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
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    m_shouldCallCheckCompleted = false;

    // Have we completed before?
    if (m_isComplete)
        return;

    // FIXME: It would be better if resource loads were kicked off after render tree update (or didn't complete synchronously).
    //        https://bugs.webkit.org/show_bug.cgi?id=171729
    if (m_frame.document()->inRenderTreeUpdate()) {
        scheduleCheckCompleted();
        return;
    }

    // Are we still parsing?
    if (m_frame.document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (m_frame.document()->cachedResourceLoader().requestCount())
        return;

    // Still waiting for elements that don't go through a FrameLoader?
    if (m_frame.document()->isDelayingLoadEvent())
        return;

    auto* scriptableParser = m_frame.document()->scriptableDocumentParser();
    if (scriptableParser && scriptableParser->hasScriptsWaitingForStylesheets())
        return;

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // Important not to protect earlier in this function, because earlier parts
    // of this function can be called in the frame's destructor, and it's not legal
    // to ref an object while it's being destroyed.
    Ref<Frame> protect(m_frame);

    // OK, completed.
    m_isComplete = true;
    m_requestedHistoryItem = nullptr;
    m_frame.document()->setReadyState(Document::Complete);

    checkCallImplicitClose(); // if we didn't do it before

    m_frame.navigationScheduler().startTimer();

    completed();
    if (m_frame.page())
        checkLoadComplete();
}

void FrameLoader::checkTimerFired()
{
    checkCompletenessNow();
}

void FrameLoader::checkCompletenessNow()
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
    m_checkTimer.startOneShot(0_s);
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
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadURLIntoChildFrame: frame load started");

    ASSERT(childFrame);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (auto activeLoader = activeDocumentLoader()) {
        if (auto subframeArchive = activeLoader->popArchiveForSubframe(childFrame->tree().uniqueName(), url)) {
            childFrame->loader().loadArchive(RefPtr<Archive> { subframeArchive }.releaseNonNull());
            return;
        }
    }
#endif

    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    auto* parentItem = history().currentItem();
    if (parentItem && parentItem->children().size() && isBackForwardLoadType(loadType()) && !m_frame.document()->loadEventFinished()) {
        if (auto* childItem = parentItem->childItemWithTarget(childFrame->tree().uniqueName())) {
            childFrame->loader().m_requestedHistoryItem = childItem;
            childFrame->loader().loadDifferentDocumentItem(*childItem, nullptr, loadType(), MayAttemptCacheOnlyLoadForFormSubmissionItem, ShouldTreatAsContinuingLoad::No);
            return;
        }
    }

    auto* lexicalFrame = lexicalFrameFromCommonVM();
    auto initiatedByMainFrame = lexicalFrame && lexicalFrame->isMainFrame() ? InitiatedByMainFrame::Yes : InitiatedByMainFrame::Unknown;

    FrameLoadRequest frameLoadRequest { *m_frame.document(), m_frame.document()->securityOrigin(), { url }, "_self"_s, initiatedByMainFrame };
    frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);
    frameLoadRequest.setLockBackForwardList(LockBackForwardList::Yes);
    childFrame->loader().loadURL(WTFMove(frameLoadRequest), referer, FrameLoadType::RedirectWithLockedBackForwardList, nullptr, { }, WTF::nullopt, [] { });
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

void FrameLoader::loadArchive(Ref<Archive>&& archive)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadArchive: frame load started");

    ArchiveResource* mainResource = archive->mainResource();
    ASSERT(mainResource);
    if (!mainResource)
        return;

    ResourceResponse response(URL(), mainResource->mimeType(), mainResource->data().size(), mainResource->textEncoding());
    SubstituteData substituteData(&mainResource->data(), URL(), response, SubstituteData::SessionHistoryVisibility::Hidden);
    
    ResourceRequest request(mainResource->url());

    auto documentLoader = m_client->createDocumentLoader(request, substituteData);
    documentLoader->setArchive(WTFMove(archive));
    load(documentLoader.get());
}

#endif // ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

String FrameLoader::outgoingReferrer() const
{
    // See http://www.whatwg.org/specs/web-apps/current-work/#fetching-resources
    // for why we walk the parent chain for srcdoc documents.
    Frame* frame = &m_frame;
    while (frame && frame->document()->isSrcdocDocument()) {
        frame = frame->tree().parent();
        // Srcdoc documents cannot be top-level documents, by definition,
        // because they need to be contained in iframes with the srcdoc.
        ASSERT(frame);
    }
    if (!frame)
        return emptyString();
    return frame->loader().m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    return m_frame.document()->securityOrigin().toString();
}

bool FrameLoader::checkIfFormActionAllowedByCSP(const URL& url, bool didReceiveRedirectResponse) const
{
    if (m_submittedFormURL.isEmpty())
        return true;

    auto redirectResponseReceived = didReceiveRedirectResponse ? ContentSecurityPolicy::RedirectResponseReceived::Yes : ContentSecurityPolicy::RedirectResponseReceived::No;
    return m_frame.document()->contentSecurityPolicy()->allowFormAction(url, redirectResponseReceived);
}

Frame* FrameLoader::opener()
{
    return m_opener;
}

void FrameLoader::setOpener(Frame* opener)
{
    if (m_opener && !opener)
        m_client->didDisownOpener();

    if (m_opener) {
        // When setOpener is called in ~FrameLoader, opener's m_frameLoader is already cleared.
        auto& openerFrameLoader = m_opener == &m_frame ? *this : m_opener->loader();
        openerFrameLoader.m_openedFrames.remove(&m_frame);
    }
    if (opener) {
        opener->loader().m_openedFrames.add(&m_frame);
        if (auto* page = m_frame.page())
            page->setOpenedByDOMWithOpener();
    }
    m_opener = opener;

    if (m_frame.document())
        m_frame.document()->initSecurityContext();
}

void FrameLoader::provisionalLoadStarted()
{
    if (m_stateMachine.firstLayoutDone())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::CommittedFirstRealLoad);
    m_frame.navigationScheduler().cancel(NewLoadInProgress::Yes);
    m_client->provisionalLoadStarted();

    if (m_frame.isMainFrame()) {
        tracePoint(MainResourceLoadDidStartProvisional);

        if (auto* page = m_frame.page())
            page->didStartProvisionalLoad();
    }
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

    RegistrableDomain registrableDomain(url);
    for (Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext(&m_frame)) {
        if (SecurityPolicy::shouldInheritSecurityOriginFromOwner(frame->document()->url()) || registrableDomain.matches(frame->document()->url()))
            frame->document()->setSiteForCookies(url);
    }
}

// This does the same kind of work that didOpenURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::loadInSameDocument(const URL& url, SerializedScriptValue* stateObject, bool isNewNavigation)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadInSameDocument: frame load started");

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

    bool hashChange = equalIgnoringFragmentIdentifier(url, oldURL) && !equalRespectingNullity(url.fragmentIdentifier(), oldURL.fragmentIdentifier());

    history().updateForSameDocumentNavigation();

    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    if (hashChange)
        m_frame.eventHandler().stopAutoscrollTimer();
    
    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    started();

    if (auto* ownerElement = m_frame.ownerElement()) {
        auto* ownerRenderer = ownerElement->renderer();
        auto* view = m_frame.view();
        if (is<RenderWidget>(ownerRenderer) && view)
            downcast<RenderWidget>(*ownerRenderer).setWidget(view);
    }

    // We need to scroll to the fragment whether or not a hash change occurred, since
    // the user might have scrolled since the previous navigation.
    scrollToFragmentWithParentBoundary(url, isNewNavigation);
    
    m_isComplete = false;
    checkCompleted();

    if (isNewNavigation) {
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for fragment
        // navigation we'll not go through a real load and reach Completed state.
        checkLoadComplete();
    }

    m_client->dispatchDidNavigateWithinPage();

    m_frame.document()->statePopped(stateObject ? Ref<SerializedScriptValue> { *stateObject } : SerializedScriptValue::nullValue());
    m_client->dispatchDidPopStateWithinPage();
    
    if (hashChange) {
        m_frame.document()->enqueueHashchangeEvent(oldURL.string(), url.string());
        m_client->dispatchDidChangeLocationWithinPage();
    }
    
    // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
    m_client->didFinishLoad();
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
        m_frame.view()->maintainScrollPositionAtAnchor(nullptr);
}

void FrameLoader::started()
{
    for (Frame* frame = &m_frame; frame; frame = frame->tree().parent())
        frame->loader().m_isComplete = false;
}

void FrameLoader::prepareForLoadStart()
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "prepareForLoadStart: Starting frame load");

    m_progressTracker->progressStarted();
    m_client->dispatchDidStartProvisionalLoad();

    if (AXObjectCache::accessibilityEnabled()) {
        if (AXObjectCache* cache = m_frame.document()->existingAXObjectCache()) {
            AXObjectCache::AXLoadingEvent loadingEvent = loadType() == FrameLoadType::Reload ? AXObjectCache::AXLoadingReloaded : AXObjectCache::AXLoadingStarted;
            cache->frameLoadingEventNotification(&m_frame, loadingEvent);
        }
    }
}

void FrameLoader::setupForReplace()
{
    m_client->revertToProvisionalState(m_documentLoader.get());
    setState(FrameStateProvisional);
    m_provisionalDocumentLoader = m_documentLoader;
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "setupForReplace: Setting provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    m_documentLoader = nullptr;
    detachChildren();
}

void FrameLoader::loadFrameRequest(FrameLoadRequest&& request, Event* event, RefPtr<FormState>&& formState, Optional<AdClickAttribution>&& adClickAttribution)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadFrameRequest: frame load started");

    // Protect frame from getting blown away inside dispatchBeforeLoadEvent in loadWithDocumentLoader.
    auto protectFrame = makeRef(m_frame);

    URL url = request.resourceRequest().url();

    ASSERT(m_frame.document());
    if (!request.requesterSecurityOrigin().canDisplay(url)) {
        reportLocalLoadFailed(&m_frame, url.stringCenterEllipsizedToLength());
        return;
    }

    String argsReferrer = request.resourceRequest().httpReferrer();
    if (argsReferrer.isEmpty())
        argsReferrer = outgoingReferrer();

    ReferrerPolicy referrerPolicy = request.referrerPolicy();
    if (referrerPolicy == ReferrerPolicy::EmptyString)
        referrerPolicy = m_frame.document()->referrerPolicy();
    String referrer = SecurityPolicy::generateReferrerHeader(referrerPolicy, url, argsReferrer);

    FrameLoadType loadType;
    if (request.resourceRequest().cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData)
        loadType = FrameLoadType::Reload;
    else if (request.lockBackForwardList() == LockBackForwardList::Yes)
        loadType = FrameLoadType::RedirectWithLockedBackForwardList;
    else
        loadType = FrameLoadType::Standard;

    auto completionHandler = [this, protectedFrame = makeRef(m_frame), formState = makeWeakPtr(formState.get()), frameName = request.frameName()] {
        // FIXME: It's possible this targetFrame will not be the same frame that was targeted by the actual
        // load if frame names have changed.
        Frame* sourceFrame = formState ? formState->sourceDocument().frame() : &m_frame;
        if (!sourceFrame)
            sourceFrame = &m_frame;
        Frame* targetFrame = sourceFrame->loader().findFrameForNavigation(frameName);
        if (targetFrame && targetFrame != sourceFrame) {
            if (Page* page = targetFrame->page())
                page->chrome().focus();
        }
    };

    if (request.resourceRequest().httpMethod() == "POST")
        loadPostRequest(WTFMove(request), referrer, loadType, event, WTFMove(formState), WTFMove(completionHandler));
    else
        loadURL(WTFMove(request), referrer, loadType, event, WTFMove(formState), WTFMove(adClickAttribution), WTFMove(completionHandler));
}

static ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicyToApply(Frame& currentFrame, InitiatedByMainFrame initiatedByMainFrame, ShouldOpenExternalURLsPolicy propagatedPolicy)
{
    if (UserGestureIndicator::processingUserGesture())
        return ShouldOpenExternalURLsPolicy::ShouldAllow;

    if (initiatedByMainFrame == InitiatedByMainFrame::Yes)
        return propagatedPolicy;

    if (!currentFrame.isMainFrame())
        return ShouldOpenExternalURLsPolicy::ShouldNotAllow;

    return propagatedPolicy;
}

static ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicyToApply(Frame& currentFrame, const FrameLoadRequest& frameLoadRequest)
{
    return shouldOpenExternalURLsPolicyToApply(currentFrame, frameLoadRequest.initiatedByMainFrame(), frameLoadRequest.shouldOpenExternalURLsPolicy());
}

static void applyShouldOpenExternalURLsPolicyToNewDocumentLoader(Frame& frame, DocumentLoader& documentLoader, InitiatedByMainFrame initiatedByMainFrame, ShouldOpenExternalURLsPolicy propagatedPolicy)
{
    documentLoader.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicyToApply(frame, initiatedByMainFrame, propagatedPolicy));
}

static void applyShouldOpenExternalURLsPolicyToNewDocumentLoader(Frame& frame, DocumentLoader& documentLoader, const FrameLoadRequest& frameLoadRequest)
{
    documentLoader.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicyToApply(frame, frameLoadRequest));
}

bool FrameLoader::isNavigationAllowed() const
{
    return m_pageDismissalEventBeingDispatched == PageDismissalType::None && !m_frame.script().willReplaceWithResultOfExecutingJavascriptURL() && NavigationDisabler::isNavigationAllowed(m_frame);
}

bool FrameLoader::isStopLoadingAllowed() const
{
    return m_pageDismissalEventBeingDispatched == PageDismissalType::None;
}

void FrameLoader::loadURL(FrameLoadRequest&& frameLoadRequest, const String& referrer, FrameLoadType newLoadType, Event* event, RefPtr<FormState>&& formState, Optional<AdClickAttribution>&& adClickAttribution, CompletionHandler<void()>&& completionHandler)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadURL: frame load started");
    ASSERT(frameLoadRequest.resourceRequest().httpMethod() == "GET");

    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));
    if (m_inStopAllLoaders || m_inClearProvisionalLoadForPolicyCheck)
        return;

    Ref<Frame> protect(m_frame);

    // Anchor target is ignored when the download attribute is set since it will download the hyperlink rather than follow it.
    String effectiveFrameName = frameLoadRequest.downloadAttribute().isNull() ? frameLoadRequest.frameName() : String();
    bool isFormSubmission = formState;

    // The search for a target frame is done earlier in the case of form submission.
    auto targetFrame = isFormSubmission ? nullptr : makeRefPtr(findFrameForNavigation(effectiveFrameName));
    if (targetFrame && targetFrame != &m_frame) {
        frameLoadRequest.setFrameName("_self");
        targetFrame->loader().loadURL(WTFMove(frameLoadRequest), referrer, newLoadType, event, WTFMove(formState), WTFMove(adClickAttribution), completionHandlerCaller.release());
        return;
    }

    const URL& newURL = frameLoadRequest.resourceRequest().url();
    ResourceRequest request(newURL);
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);

    addExtraFieldsToRequest(request, IsMainResource::Yes, newLoadType);

    ASSERT(newLoadType != FrameLoadType::Same);

    if (!isNavigationAllowed())
        return;

    NavigationAction action { frameLoadRequest.requester(), request, frameLoadRequest.initiatedByMainFrame(), newLoadType, isFormSubmission, event, frameLoadRequest.shouldOpenExternalURLsPolicy(), frameLoadRequest.downloadAttribute() };
    action.setLockHistory(frameLoadRequest.lockHistory());
    action.setLockBackForwardList(frameLoadRequest.lockBackForwardList());
    if (adClickAttribution && m_frame.isMainFrame())
        action.setAdClickAttribution(WTFMove(*adClickAttribution));

    NewFrameOpenerPolicy openerPolicy = frameLoadRequest.newFrameOpenerPolicy();
    AllowNavigationToInvalidURL allowNavigationToInvalidURL = frameLoadRequest.allowNavigationToInvalidURL();
    if (!targetFrame && !effectiveFrameName.isEmpty()) {
        action = action.copyWithShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicyToApply(m_frame, frameLoadRequest));
        policyChecker().checkNewWindowPolicy(WTFMove(action), WTFMove(request), WTFMove(formState), effectiveFrameName, [this, allowNavigationToInvalidURL, openerPolicy, completionHandler = completionHandlerCaller.release()] (const ResourceRequest& request, WeakPtr<FormState>&& formState, const String& frameName, const NavigationAction& action, ShouldContinuePolicyCheck shouldContinue) mutable {
            continueLoadAfterNewWindowPolicy(request, formState.get(), frameName, action, shouldContinue, allowNavigationToInvalidURL, openerPolicy);
            completionHandler();
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
        oldDocumentLoader->setTriggeringAction(WTFMove(action));
        oldDocumentLoader->setLastCheckedRequest(ResourceRequest());
        policyChecker().stopCheck();
        policyChecker().setLoadType(newLoadType);
        RELEASE_ASSERT(!isBackForwardLoadType(newLoadType) || history().provisionalItem());
        policyChecker().checkNavigationPolicy(WTFMove(request), ResourceResponse { } /* redirectResponse */, oldDocumentLoader.get(), WTFMove(formState), [this, protectedFrame = makeRef(m_frame)] (const ResourceRequest& request, WeakPtr<FormState>&&, NavigationPolicyDecision navigationPolicyDecision) {
            continueFragmentScrollAfterNavigationPolicy(request, navigationPolicyDecision == NavigationPolicyDecision::ContinueLoad);
        }, PolicyDecisionMode::Synchronous);
        return;
    }

    // Must grab this now, since this load may stop the previous load and clear this flag.
    bool isRedirect = m_quickRedirectComing;
#if USE(SYSTEM_PREVIEW)
    bool isSystemPreview = frameLoadRequest.isSystemPreview();
    if (isSystemPreview)
        request.setSystemPreviewInfo(frameLoadRequest.systemPreviewInfo());
#endif
    loadWithNavigationAction(request, WTFMove(action), newLoadType, WTFMove(formState), allowNavigationToInvalidURL, [this, isRedirect, sameURL, newLoadType, protectedFrame = makeRef(m_frame), completionHandler = completionHandlerCaller.release()] () mutable {
        if (isRedirect) {
            m_quickRedirectComing = false;
            if (m_provisionalDocumentLoader)
                m_provisionalDocumentLoader->setIsClientRedirect(true);
            else if (m_policyDocumentLoader)
                m_policyDocumentLoader->setIsClientRedirect(true);
        } else if (sameURL && !isReload(newLoadType)) {
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            m_loadType = FrameLoadType::Same;
        }
        completionHandler();
    });
}

SubstituteData FrameLoader::defaultSubstituteDataForURL(const URL& url)
{
    if (!shouldTreatURLAsSrcdocDocument(url))
        return SubstituteData();
    auto& srcdoc = m_frame.ownerElement()->attributeWithoutSynchronization(srcdocAttr);
    ASSERT(!srcdoc.isNull());
    CString encodedSrcdoc = srcdoc.string().utf8();

    ResourceResponse response(URL(), "text/html"_s, encodedSrcdoc.length(), "UTF-8"_s);
    return SubstituteData(SharedBuffer::create(encodedSrcdoc.data(), encodedSrcdoc.length()), URL(), response, SubstituteData::SessionHistoryVisibility::Hidden);
}

void FrameLoader::load(FrameLoadRequest&& request)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "load (FrameLoadRequest): frame load started");

    if (m_inStopAllLoaders || m_inClearProvisionalLoadForPolicyCheck)
        return;

    if (!request.frameName().isEmpty()) {
        Frame* frame = findFrameForNavigation(request.frameName());
        if (frame) {
            request.setShouldCheckNewWindowPolicy(false);
            if (&frame->loader() != this) {
                frame->loader().load(WTFMove(request));
                return;
            }
        }
    }

    if (request.shouldCheckNewWindowPolicy()) {
        NavigationAction action { request.requester(), request.resourceRequest(), InitiatedByMainFrame::Unknown, NavigationType::Other, request.shouldOpenExternalURLsPolicy() };
        policyChecker().checkNewWindowPolicy(WTFMove(action), WTFMove(request.resourceRequest()), { }, request.frameName(), [this] (const ResourceRequest& request, WeakPtr<FormState>&& formState, const String& frameName, const NavigationAction& action, ShouldContinuePolicyCheck shouldContinue) {
            continueLoadAfterNewWindowPolicy(request, formState.get(), frameName, action, shouldContinue, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Suppress);
        });

        return;
    }

    if (!request.hasSubstituteData())
        request.setSubstituteData(defaultSubstituteDataForURL(request.resourceRequest().url()));

    Ref<DocumentLoader> loader = m_client->createDocumentLoader(request.resourceRequest(), request.substituteData());
    loader->setAllowsWebArchiveForMainFrame(request.isRequestFromClientOrUserInput());
    loader->setAllowsDataURLsForMainFrame(request.isRequestFromClientOrUserInput());
    addSameSiteInfoToRequestIfNeeded(loader->request());
    applyShouldOpenExternalURLsPolicyToNewDocumentLoader(m_frame, loader, request);

    if (request.shouldTreatAsContinuingLoad()) {
        loader->setClientRedirectSourceForHistory(request.clientRedirectSourceForHistory());
        if (request.lockBackForwardList() == LockBackForwardList::Yes) {
            loader->setIsClientRedirect(true);
            m_loadType = FrameLoadType::RedirectWithLockedBackForwardList;
        }
    }

    SetForScope<LoadContinuingState> continuingLoadGuard(m_currentLoadContinuingState, request.shouldTreatAsContinuingLoad() ? LoadContinuingState::ContinuingWithRequest : LoadContinuingState::NotContinuing);
    load(loader.get());
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, NavigationAction&& action, FrameLoadType type, RefPtr<FormState>&& formState, AllowNavigationToInvalidURL allowNavigationToInvalidURL, CompletionHandler<void()>&& completionHandler)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadWithNavigationAction: frame load started");

    Ref<DocumentLoader> loader = m_client->createDocumentLoader(request, defaultSubstituteDataForURL(request.url()));
    applyShouldOpenExternalURLsPolicyToNewDocumentLoader(m_frame, loader, action.initiatedByMainFrame(), action.shouldOpenExternalURLsPolicy());

    if (action.lockHistory() == LockHistory::Yes && m_documentLoader)
        loader->setClientRedirectSourceForHistory(m_documentLoader->didCreateGlobalHistoryEntry() ? m_documentLoader->urlForHistory().string() : m_documentLoader->clientRedirectSourceForHistory());

    loader->setTriggeringAction(WTFMove(action));
    if (m_documentLoader)
        loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    loadWithDocumentLoader(loader.ptr(), type, WTFMove(formState), allowNavigationToInvalidURL, WTFMove(completionHandler));
}

void FrameLoader::load(DocumentLoader& newDocumentLoader)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "load (DocumentLoader): frame load started");

    ResourceRequest& r = newDocumentLoader.request();
    // FIXME: Using m_loadType seems wrong here.
    // If we are only preparing to load the main resource, that is previous load's load type!
    addExtraFieldsToRequest(r, IsMainResource::Yes, m_loadType);
    FrameLoadType type;

    if (shouldTreatURLAsSameAsCurrent(newDocumentLoader.originalRequest().url())) {
        r.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);
        type = FrameLoadType::Same;
    } else if (shouldTreatURLAsSameAsCurrent(newDocumentLoader.unreachableURL()) && isReload(m_loadType))
        type = m_loadType;
    else if (m_loadType == FrameLoadType::RedirectWithLockedBackForwardList && ((!newDocumentLoader.unreachableURL().isEmpty() && newDocumentLoader.substituteData().isValid()) || shouldTreatCurrentLoadAsContinuingLoad()))
        type = FrameLoadType::RedirectWithLockedBackForwardList;
    else
        type = FrameLoadType::Standard;

    if (m_documentLoader)
        newDocumentLoader.setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the history list, we treat it as a reload so the history list 
    // is appropriately maintained.
    //
    // FIXME: This seems like a dangerous overloading of the meaning of "FrameLoadType::Reload" ...
    // shouldn't a more explicit type of reload be defined, that means roughly 
    // "load without affecting history" ? 
    if (shouldReloadToHandleUnreachableURL(newDocumentLoader)) {
        // shouldReloadToHandleUnreachableURL returns true only when the original load type is back-forward.
        // In this case we should save the document state now. Otherwise the state can be lost because load type is
        // changed and updateForBackForwardNavigation() will not be called when loading is committed.
        history().saveDocumentAndScrollState();

        ASSERT(type == FrameLoadType::Standard);
        type = FrameLoadType::Reload;
    }

    loadWithDocumentLoader(&newDocumentLoader, type, nullptr, AllowNavigationToInvalidURL::Yes);
}

void FrameLoader::loadWithDocumentLoader(DocumentLoader* loader, FrameLoadType type, RefPtr<FormState>&& formState, AllowNavigationToInvalidURL allowNavigationToInvalidURL, CompletionHandler<void()>&& completionHandler)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadWithDocumentLoader: frame load started");

    // Retain because dispatchBeforeLoadEvent may release the last reference to it.
    Ref<Frame> protect(m_frame);

    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

    ASSERT(m_client->hasWebView());

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT(m_frame.view());

    if (!isNavigationAllowed())
        return;

    if (m_frame.document())
        m_previousURL = m_frame.document()->url();

    const URL& newURL = loader->request().url();

    // Only the first iframe navigation or the first iframe navigation after about:blank should be reported.
    // https://www.w3.org/TR/resource-timing-2/#resources-included-in-the-performanceresourcetiming-interface
    if (m_shouldReportResourceTimingToParentFrame && !m_previousURL.isNull() && m_previousURL != aboutBlankURL())
        m_shouldReportResourceTimingToParentFrame = false;

    // Log main frame navigation types.
    if (m_frame.isMainFrame()) {
        if (auto* page = m_frame.page()) {
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadWithDocumentLoader: main frame load started");
            page->mainFrameLoadStarted(newURL, type);
            page->performanceLogging().didReachPointOfInterest(PerformanceLogging::MainFrameLoadStarted);
        }
    }

    policyChecker().setLoadType(type);
    RELEASE_ASSERT(!isBackForwardLoadType(type) || history().provisionalItem());
    bool isFormSubmission = formState;

    const String& httpMethod = loader->request().httpMethod();

    if (shouldPerformFragmentNavigation(isFormSubmission, httpMethod, policyChecker().loadType(), newURL)) {
        RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;
        NavigationAction action { *m_frame.document(), loader->request(), InitiatedByMainFrame::Unknown, policyChecker().loadType(), isFormSubmission };

        oldDocumentLoader->setTriggeringAction(WTFMove(action));
        oldDocumentLoader->setLastCheckedRequest(ResourceRequest());
        policyChecker().stopCheck();
        RELEASE_ASSERT(!isBackForwardLoadType(policyChecker().loadType()) || history().provisionalItem());
        policyChecker().checkNavigationPolicy(ResourceRequest(loader->request()), ResourceResponse { }  /* redirectResponse */, oldDocumentLoader.get(), WTFMove(formState), [this, protectedFrame = makeRef(m_frame)] (const ResourceRequest& request, WeakPtr<FormState>&&, NavigationPolicyDecision navigationPolicyDecision) {
            continueFragmentScrollAfterNavigationPolicy(request, navigationPolicyDecision == NavigationPolicyDecision::ContinueLoad);
        }, PolicyDecisionMode::Synchronous);
        return;
    }

    if (Frame* parent = m_frame.tree().parent())
        loader->setOverrideEncoding(parent->loader().documentLoader()->overrideEncoding());

    policyChecker().stopCheck();
    setPolicyDocumentLoader(loader);
    if (loader->triggeringAction().isEmpty())
        loader->setTriggeringAction({ *m_frame.document(), loader->request(), InitiatedByMainFrame::Unknown, policyChecker().loadType(), isFormSubmission });

    if (Element* ownerElement = m_frame.ownerElement()) {
        // We skip dispatching the beforeload event if we've already
        // committed a real document load because the event would leak
        // subsequent activity by the frame which the parent frame isn't
        // supposed to learn. For example, if the child frame navigated to
        // a new URL, the parent frame shouldn't learn the URL.
        if (!m_stateMachine.committedFirstRealDocumentLoad()
            && !ownerElement->dispatchBeforeLoadEvent(loader->request().url().string())) {
            continueLoadAfterNavigationPolicy(loader->request(), formState.get(), NavigationPolicyDecision::IgnoreLoad, allowNavigationToInvalidURL);
            return;
        }
    }

    m_frame.navigationScheduler().cancel(NewLoadInProgress::Yes);

    if (shouldTreatCurrentLoadAsContinuingLoad()) {
        continueLoadAfterNavigationPolicy(loader->request(), formState.get(), NavigationPolicyDecision::ContinueLoad, allowNavigationToInvalidURL);
        return;
    }

    RELEASE_ASSERT(!isBackForwardLoadType(policyChecker().loadType()) || history().provisionalItem());
    policyChecker().checkNavigationPolicy(ResourceRequest(loader->request()), ResourceResponse { } /* redirectResponse */, loader, WTFMove(formState), [this, protectedFrame = makeRef(m_frame), allowNavigationToInvalidURL, completionHandler = completionHandlerCaller.release()] (const ResourceRequest& request, WeakPtr<FormState>&& formState, NavigationPolicyDecision navigationPolicyDecision) mutable {
        continueLoadAfterNavigationPolicy(request, formState.get(), navigationPolicyDecision, allowNavigationToInvalidURL);
        completionHandler();
    }, PolicyDecisionMode::Asynchronous);
}

void FrameLoader::clearProvisionalLoadForPolicyCheck()
{
    if (!m_policyDocumentLoader || !m_provisionalDocumentLoader || m_inClearProvisionalLoadForPolicyCheck)
        return;

    SetForScope<bool> change(m_inClearProvisionalLoadForPolicyCheck, true);
    m_provisionalDocumentLoader->stopLoading();
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "clearProvisionalLoadForPolicyCheck: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(nullptr);
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

bool FrameLoader::willLoadMediaElementURL(URL& url, Node& initiatorNode)
{
#if PLATFORM(IOS_FAMILY)
    // MobileStore depends on the iOS 4.0 era client delegate method because webView:resource:willSendRequest:redirectResponse:fromDataSource
    // doesn't let them tell when a load request is coming from a media element. See <rdar://problem/8266916> for more details.
    if (IOSApplication::isMobileStore())
        return m_client->shouldLoadMediaElementURL(url);
#endif

    ResourceRequest request(url);
    request.setInspectorInitiatorNodeIdentifier(InspectorInstrumentation::identifierForNode(initiatorNode));

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, request, ResourceResponse(url, String(), -1, String()), 0, -1, -1, error);

    url = request.url();

    return error.isNull();
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(DocumentLoader& docLoader)
{
    URL unreachableURL = docLoader.unreachableURL();

    if (unreachableURL.isEmpty())
        return false;

    if (!isBackForwardLoadType(policyChecker().loadType()))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    if (policyChecker().delegateIsDecidingNavigationPolicy() || policyChecker().delegateIsHandlingUnimplementablePolicy())
        return m_policyDocumentLoader && unreachableURL == m_policyDocumentLoader->request().url();

    return unreachableURL == m_provisionalLoadErrorBeingHandledURL;
}

void FrameLoader::reloadWithOverrideEncoding(const String& encoding)
{
    if (!m_documentLoader)
        return;

    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "reloadWithOverrideEncoding: frame load started");

    ResourceRequest request = m_documentLoader->request();
    URL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        request.setURL(unreachableURL);

    // FIXME: If the resource is a result of form submission and is not cached, the form will be silently resubmitted.
    // We should ask the user for confirmation in this case.
    request.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataElseLoad);

    Ref<DocumentLoader> loader = m_client->createDocumentLoader(request, defaultSubstituteDataForURL(request.url()));
    applyShouldOpenExternalURLsPolicyToNewDocumentLoader(m_frame, loader, InitiatedByMainFrame::Unknown, m_documentLoader->shouldOpenExternalURLsPolicyToPropagate());

    setPolicyDocumentLoader(loader.ptr());

    loader->setOverrideEncoding(encoding);

    loadWithDocumentLoader(loader.ptr(), FrameLoadType::Reload, { }, AllowNavigationToInvalidURL::Yes);
}

void FrameLoader::reload(OptionSet<ReloadOption> options)
{
    if (!m_documentLoader)
        return;

    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if (m_documentLoader->request().url().isEmpty())
        return;

    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "reload: frame load started");

    // Replace error-page URL with the URL we were trying to reach.
    ResourceRequest initialRequest = m_documentLoader->request();
    URL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        initialRequest.setURL(unreachableURL);

    // Create a new document loader for the reload, this will become m_documentLoader eventually,
    // but first it has to be the "policy" document loader, and then the "provisional" document loader.
    Ref<DocumentLoader> loader = m_client->createDocumentLoader(initialRequest, defaultSubstituteDataForURL(initialRequest.url()));
    loader->setAllowsWebArchiveForMainFrame(m_documentLoader->allowsWebArchiveForMainFrame());
    loader->setAllowsDataURLsForMainFrame(m_documentLoader->allowsDataURLsForMainFrame());
    applyShouldOpenExternalURLsPolicyToNewDocumentLoader(m_frame, loader, InitiatedByMainFrame::Unknown, m_documentLoader->shouldOpenExternalURLsPolicyToPropagate());

    loader->setUserContentExtensionsEnabled(!options.contains(ReloadOption::DisableContentBlockers));
    
    ResourceRequest& request = loader->request();

    // FIXME: We don't have a mechanism to revalidate the main resource without reloading at the moment.
    request.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);

    addSameSiteInfoToRequestIfNeeded(request);

    // If we're about to re-post, set up action so the application can warn the user.
    if (request.httpMethod() == "POST")
        loader->setTriggeringAction({ *m_frame.document(), request, InitiatedByMainFrame::Unknown, NavigationType::FormResubmitted });

    loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    auto frameLoadTypeForReloadOptions = [] (auto options) {
        if (options & ReloadOption::FromOrigin)
            return FrameLoadType::ReloadFromOrigin;
        if (options & ReloadOption::ExpiredOnly)
            return FrameLoadType::ReloadExpiredOnly;
        return FrameLoadType::Reload;
    };
    
    loadWithDocumentLoader(loader.ptr(), frameLoadTypeForReloadOptions(options), { }, AllowNavigationToInvalidURL::Yes);
}

void FrameLoader::stopAllLoaders(ClearProvisionalItemPolicy clearProvisionalItemPolicy, StopLoadingPolicy stopLoadingPolicy)
{
    if (m_frame.document() && m_frame.document()->backForwardCacheState() == Document::InBackForwardCache)
        return;

    if (stopLoadingPolicy == StopLoadingPolicy::PreventDuringUnloadEvents && !isStopLoadingAllowed())
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;

    // This method might dispatch events.
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());

    // Calling stopLoading() on the provisional document loader can blow away
    // the frame from underneath.
    Ref<Frame> protect(m_frame);

    m_inStopAllLoaders = true;

    policyChecker().stopCheck();

    // If no new load is in progress, we should clear the provisional item from history
    // before we call stopLoading.
    if (clearProvisionalItemPolicy == ShouldClearProvisionalItem)
        history().setProvisionalItem(nullptr);

    for (RefPtr<Frame> child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->loader().stopAllLoaders(clearProvisionalItemPolicy);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();
    if (m_frame.page() && !m_frame.page()->chrome().client().isSVGImageChromeClient())
        platformStrategies()->loaderStrategy()->browsingContextRemoved(frame());

    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "stopAllLoaders: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(nullptr);

    m_inStopAllLoaders = false;    
}

void FrameLoader::stopForBackForwardCache()
{
    // Stop provisional loads in subframes (The one in the main frame is about to be committed).
    if (!m_frame.isMainFrame()) {
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->stopLoading();
        FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "stopForBackForwardCache: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
        setProvisionalDocumentLoader(nullptr);
    }

    // Stop current loads.
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    for (RefPtr<Frame> child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling())
        child->loader().stopForBackForwardCache();

    // We cancel pending navigations & policy checks *after* cancelling loads because cancelling loads might end up
    // running script, which could schedule new navigations.
    policyChecker().stopCheck();
    m_frame.navigationScheduler().cancel();
}

void FrameLoader::stopAllLoadersAndCheckCompleteness()
{
    stopAllLoaders();

    if (!m_checkTimer.isActive())
        return;

    m_checkTimer.stop();
    m_checkingLoadCompleteForDetachment = true;
    checkCompletenessNow();
    m_checkingLoadCompleteForDetachment = false;
}

void FrameLoader::stopForUserCancel(bool deferCheckLoadComplete)
{
    // Calling stopAllLoaders can cause the frame to be deallocated, including the frame loader.
    Ref<Frame> protectedFrame(m_frame);

    stopAllLoaders();

#if PLATFORM(IOS_FAMILY)
    // Lay out immediately when stopping to immediately clear the old page if we just committed this one
    // but haven't laid out/painted yet.
    // FIXME: Is this behavior specific to iOS? Or should we expose a setting to toggle this behavior?
    if (m_frame.view() && !m_frame.view()->didFirstLayout())
        m_frame.view()->layoutContext().layout();
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

    if (loader == m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    m_client->prepareForDataSourceReplacement();
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
        loader->attachToFrame(m_frame);
    if (m_policyDocumentLoader
            && m_policyDocumentLoader != m_provisionalDocumentLoader
            && m_policyDocumentLoader != m_documentLoader)
        m_policyDocumentLoader->detachFromFrame();

    m_policyDocumentLoader = loader;
}

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "setProvisionalDocumentLoader: Setting provisional document loader (m_provisionalDocumentLoader=%p)", loader);

    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
}

void FrameLoader::setState(FrameState newState)
{
    FrameState oldState = m_state;
    m_state = newState;
    
    if (newState == FrameStateProvisional)
        provisionalLoadStarted();
    else if (newState == FrameStateComplete) {
        frameLoadCompleted();
        if (m_documentLoader)
            m_documentLoader->stopRecordingResponses();
        if (m_frame.isMainFrame() && oldState != newState) {
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "setState: main frame load completed");
            m_frame.page()->performanceLogging().didReachPointOfInterest(PerformanceLogging::MainFrameLoadCompleted);
        }
    }
}

void FrameLoader::clearProvisionalLoad()
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "clearProvisionalLoad: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(nullptr);
    m_progressTracker->progressCompleted();
    setState(FrameStateComplete);
}

void FrameLoader::commitProvisionalLoad()
{
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    Ref<Frame> protect(m_frame);

    std::unique_ptr<CachedPage> cachedPage;
    if (m_loadingFromCachedPage && history().provisionalItem())
        cachedPage = BackForwardCache::singleton().take(*history().provisionalItem(), m_frame.page());

    LOG(BackForwardCache, "WebCoreLoading %s: About to commit provisional load from previous URL '%s' to new URL '%s' with cached page %p", m_frame.tree().uniqueName().string().utf8().data(),
        m_frame.document() ? m_frame.document()->url().stringCenterEllipsizedToLength().utf8().data() : "",
        pdl ? pdl->url().stringCenterEllipsizedToLength().utf8().data() : "<no provisional DocumentLoader>", cachedPage.get());

    willTransitionToCommitted();

    if (!m_frame.tree().parent() && history().currentItem() && history().currentItem() != history().provisionalItem()) {
        // Check to see if we need to cache the page we are navigating away from into the back/forward cache.
        // We are doing this here because we know for sure that a new page is about to be loaded.
        BackForwardCache::singleton().addIfCacheable(*history().currentItem(), m_frame.page());
        
        WebCore::jettisonExpensiveObjectsOnTopLevelNavigation();
    }

    if (m_loadType != FrameLoadType::Replace)
        closeOldDataSources();

    if (!cachedPage && !m_stateMachine.creatingInitialEmptyDocument())
        m_client->makeRepresentation(pdl.get());

    transitionToCommitted(cachedPage.get());

    if (pdl && m_documentLoader) {
        // Check if the destination page is allowed to access the previous page's timing information.
        Ref<SecurityOrigin> securityOrigin(SecurityOrigin::create(pdl->request().url()));
        m_documentLoader->timing().setHasSameOriginAsPreviousDocument(securityOrigin.get().canRequest(m_previousURL));
    }

    // Call clientRedirectCancelledOrFinished() here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (m_sentRedirectNotification)
        clientRedirectCancelledOrFinished(NewLoadInProgress::No);
    
    if (cachedPage && cachedPage->document()) {
#if PLATFORM(IOS_FAMILY)
        // FIXME: CachedPage::restore() would dispatch viewport change notification. However UIKit expects load
        // commit to happen before any changes to viewport arguments and dealing with this there is difficult.
        m_frame.page()->chrome().setDispatchViewportDataDidChangeSuppressed(true);
#endif
        willRestoreFromCachedPage();

        // Start request for the main resource and dispatch didReceiveResponse before the load is committed for
        // consistency with all other loads. See https://bugs.webkit.org/show_bug.cgi?id=150927.
        ResourceError mainResouceError;
        unsigned long mainResourceIdentifier;
        ResourceRequest mainResourceRequest(cachedPage->documentLoader()->request());
        requestFromDelegate(mainResourceRequest, mainResourceIdentifier, mainResouceError);
        notifier().dispatchDidReceiveResponse(cachedPage->documentLoader(), mainResourceIdentifier, cachedPage->documentLoader()->response());

        auto hasInsecureContent = cachedPage->cachedMainFrame()->hasInsecureContent();
        auto usedLegacyTLS = cachedPage->cachedMainFrame()->usedLegacyTLS();

        dispatchDidCommitLoad(hasInsecureContent, usedLegacyTLS);

        // FIXME: This API should be turned around so that we ground CachedPage into the Page.
        cachedPage->restore(*m_frame.page());

#if PLATFORM(IOS_FAMILY)
        m_frame.page()->chrome().setDispatchViewportDataDidChangeSuppressed(false);
        m_frame.page()->chrome().dispatchViewportPropertiesDidChange(m_frame.page()->viewportArguments());
#endif
        m_frame.page()->chrome().dispatchDisabledAdaptationsDidChange(m_frame.page()->disabledAdaptations());

        auto& title = m_documentLoader->title();
        if (!title.string.isNull())
            m_client->dispatchDidReceiveTitle(title);

        // Send remaining notifications for the main resource.
        notifier().sendRemainingDelegateMessages(m_documentLoader.get(), mainResourceIdentifier, mainResourceRequest, ResourceResponse(),
            nullptr, static_cast<int>(m_documentLoader->response().expectedContentLength()), 0, mainResouceError);

        Vector<Ref<Frame>> targetFrames;
        targetFrames.append(m_frame);
        for (auto* child = m_frame.tree().firstChild(); child; child = child->tree().traverseNext(&m_frame))
            targetFrames.append(*child);

        for (auto& frame : targetFrames)
            frame->loader().checkCompleted();
    } else
        didOpenURL();

    LOG(Loading, "WebCoreLoading %s: Finished committing provisional load to URL %s", m_frame.tree().uniqueName().string().utf8().data(),
        m_frame.document() ? m_frame.document()->url().stringCenterEllipsizedToLength().utf8().data() : "");

    if (m_loadType == FrameLoadType::Standard && m_documentLoader && m_documentLoader->isClientRedirect())
        history().updateForClientRedirect();

    if (m_loadingFromCachedPage) {
        // Note, didReceiveDocType is expected to be called for cached pages. See <rdar://problem/5906758> for more details.
        if (auto* page = m_frame.page())
            page->chrome().didReceiveDocType(m_frame);
        m_frame.document()->resume(ReasonForSuspension::BackForwardCache);

        // Force a layout to update view size and thereby update scrollbars.
#if PLATFORM(IOS_FAMILY)
        if (!m_client->forceLayoutOnRestoreFromBackForwardCache())
            m_frame.view()->forceLayout();
#else
        m_frame.view()->forceLayout();
#endif

        // Main resource delegates were already sent, so we skip the first response here.
        for (unsigned i = 1; i < m_documentLoader->responses().size(); ++i) {
            const auto& response = m_documentLoader->responses()[i];
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
    ASSERT(m_client->hasWebView());
    ASSERT(m_state == FrameStateProvisional);

    if (m_state != FrameStateProvisional)
        return;

    if (FrameView* view = m_frame.view()) {
        if (ScrollAnimator* scrollAnimator = view->existingScrollAnimator())
            scrollAnimator->cancelAnimations();
    }

    m_client->setCopiesOnScroll();
    history().updateForCommit();

    // The call to closeURL() invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    auto originalProvisionalDocumentLoader = m_provisionalDocumentLoader;
    if (m_documentLoader)
        closeURL();
    if (originalProvisionalDocumentLoader != m_provisionalDocumentLoader)
        return;

    if (m_documentLoader)
        m_documentLoader->stopLoadingSubresources();
    if (m_documentLoader)
        m_documentLoader->stopLoadingPlugIns();

    // Setting our document loader invokes the unload event handler of our child frames.
    // Script can do anything. If the script initiates a new load, we need to abandon the
    // current load or the two will stomp each other.
    setDocumentLoader(m_provisionalDocumentLoader.get());
    if (originalProvisionalDocumentLoader != m_provisionalDocumentLoader)
        return;
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "transitionToCommitted: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(nullptr);

    // Nothing else can interrupt this commit - set the Provisional->Committed transition in stone
    setState(FrameStateCommittedPage);

    // Handle adding the URL to the back/forward list.
    auto documentLoader = m_documentLoader;

    switch (m_loadType) {
    case FrameLoadType::Forward:
    case FrameLoadType::Back:
    case FrameLoadType::IndexedBackForward:
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
                ASSERT(cachedPage->documentLoader());
                cachedPage->documentLoader()->attachToFrame(m_frame);
                m_client->transitionToCommittedFromCachedFrame(cachedPage->cachedMainFrame());
            } else
                m_client->transitionToCommittedForNewPage();
        }
        break;

    case FrameLoadType::Reload:
    case FrameLoadType::ReloadFromOrigin:
    case FrameLoadType::ReloadExpiredOnly:
    case FrameLoadType::Same:
    case FrameLoadType::Replace:
        history().updateForReload();
        m_client->transitionToCommittedForNewPage();
        break;

    case FrameLoadType::Standard:
        history().updateForStandardLoad();
        if (m_frame.view())
            m_frame.view()->setScrollbarsSuppressed(true);
        m_client->transitionToCommittedForNewPage();
        break;

    case FrameLoadType::RedirectWithLockedBackForwardList:
        history().updateForRedirectWithLockedBackForwardList();
        m_client->transitionToCommittedForNewPage();
        break;
    }

    if (documentLoader)
        documentLoader->writer().setMIMEType(documentLoader->responseMIMEType());

    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    if (!m_stateMachine.committedFirstRealDocumentLoad())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::DisplayingInitialEmptyDocumentPostCommit);
}

void FrameLoader::clientRedirectCancelledOrFinished(NewLoadInProgress newLoadInProgress)
{
    // Note that -webView:didCancelClientRedirectForFrame: is called on the frame load delegate even if
    // the redirect succeeded.  We should either rename this API, or add a new method, like
    // -webView:didFinishClientRedirectForFrame:
    m_client->dispatchDidCancelClientRedirect();

    if (newLoadInProgress == NewLoadInProgress::No)
        m_quickRedirectComing = false;

    m_sentRedirectNotification = false;
}

void FrameLoader::clientRedirected(const URL& url, double seconds, WallTime fireDate, LockBackForwardList lockBackForwardList)
{
    m_client->dispatchWillPerformClientRedirect(url, seconds, fireDate, lockBackForwardList);
    
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
        m_client->dispatchWillClose();

    m_client->setMainFrameDocumentReady(false); // stop giving out the actual DOMDocument to observers
}

void FrameLoader::willRestoreFromCachedPage()
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
    auto document = makeRef(*cachedFrame.document());
    ASSERT(document->domWindow());

    clear(document.ptr(), true, true, cachedFrame.isMainFrame());

    document->attachToCachedFrame(cachedFrame);
    document->setBackForwardCacheState(Document::NotInBackForwardCache);

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    setOutgoingReferrer(url);

    FrameView* view = cachedFrame.view();
    
    // When navigating to a CachedFrame its FrameView should never be null.  If it is we'll crash in creative ways downstream.
    ASSERT(view);
    view->setWasScrolledByUser(false);

    Optional<IntRect> previousViewFrameRect = m_frame.view() ?  m_frame.view()->frameRect() : Optional<IntRect>(WTF::nullopt);
    m_frame.setView(view);

    // Use the previous ScrollView's frame rect.
    if (previousViewFrameRect)
        view->setFrameRect(previousViewFrameRect.value());


    // Setting the document builds the render tree and runs post style resolution callbacks that can do anything,
    // including loading a child frame before its been re-attached to the frame tree as part of this restore.
    // For example, the HTML object element may load its content into a frame in a post style resolution callback.
    Style::PostResolutionCallbackDisabler disabler(document.get());
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    NavigationDisabler disableNavigation { &m_frame };
    
    m_frame.setDocument(document.copyRef());

    document->domWindow()->resumeFromBackForwardCache();

    updateFirstPartyForCookies();

    cachedFrame.restore();
}

bool FrameLoader::isReplacing() const
{
    return m_loadType == FrameLoadType::Replace;
}

void FrameLoader::setReplacing()
{
    m_loadType = FrameLoadType::Replace;
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
    m_client->willChangeTitle(loader);
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}
    
CachePolicy FrameLoader::subresourceCachePolicy(const URL& url) const
{
    if (Page* page = m_frame.page()) {
        if (page->isResourceCachingDisabledByWebInspector())
            return CachePolicyReload;
    }

    if (m_isComplete)
        return CachePolicyVerify;

    if (m_loadType == FrameLoadType::ReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame.tree().parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader().subresourceCachePolicy(url);
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }
    
    switch (m_loadType) {
    case FrameLoadType::Reload:
        return CachePolicyRevalidate;
    case FrameLoadType::Back:
    case FrameLoadType::Forward:
    case FrameLoadType::IndexedBackForward:
        return CachePolicyHistoryBuffer;
    case FrameLoadType::ReloadFromOrigin:
        ASSERT_NOT_REACHED(); // Already handled above.
        return CachePolicyReload;
    case FrameLoadType::RedirectWithLockedBackForwardList:
    case FrameLoadType::Replace:
    case FrameLoadType::Same:
    case FrameLoadType::Standard:
        return CachePolicyVerify;
    case FrameLoadType::ReloadExpiredOnly:
        // We know about expiration for HTTP and data. Do a normal reload otherwise.
        if (!url.protocolIsInHTTPFamily() && !url.protocolIsData())
            return CachePolicyReload;
        return CachePolicyVerify;
    }

    RELEASE_ASSERT_NOT_REACHED();
    return CachePolicyVerify;
}

void FrameLoader::dispatchDidFailProvisionalLoad(DocumentLoader& provisionalDocumentLoader, const ResourceError& error)
{
    m_provisionalLoadErrorBeingHandledURL = provisionalDocumentLoader.url();

#if ENABLE(CONTENT_FILTERING)
    auto contentFilter = provisionalDocumentLoader.contentFilter();
    auto contentFilterWillContinueLoading = false;
#endif

    auto willContinueLoading = WillContinueLoading::No;
    if (history().provisionalItem())
        willContinueLoading = WillContinueLoading::Yes;
#if ENABLE(CONTENT_FILTERING)
    if (contentFilter && contentFilter->willHandleProvisionalLoadFailure(error)) {
        willContinueLoading = WillContinueLoading::Yes;
        contentFilterWillContinueLoading = true;
    }
#endif

    m_client->dispatchDidFailProvisionalLoad(error, willContinueLoading);

#if ENABLE(CONTENT_FILTERING)
    if (contentFilterWillContinueLoading)
        contentFilter->handleProvisionalLoadFailure(error);
#endif

    m_provisionalLoadErrorBeingHandledURL = { };
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());

    // FIXME: Should this check be done in checkLoadComplete instead of here?
    // FIXME: Why does this one check need to be repeated here, and not the many others from checkCompleted?
    if (m_frame.document()->isDelayingLoadEvent())
        return;

    switch (m_state) {
        case FrameStateProvisional: {
            // FIXME: Prohibiting any provisional load failures from being sent to clients
            // while handling provisional load failures is too heavy. For example, the current
            // load will fail to cancel another ongoing load. That might prevent clients' page
            // load state being handled properly.
            if (!m_provisionalLoadErrorBeingHandledURL.isEmpty())
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
                FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "checkLoadCompleteForThisFrame: Failed provisional load (isTimeout = %d, isCancellation = %d, errorCode = %d)", error.isTimeout(), error.isCancellation(), error.errorCode());

                dispatchDidFailProvisionalLoad(*pdl, error);
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
                    page->backForward().setCurrentItem(*item);
                }
            return;
        }
        
        case FrameStateCommittedPage: {
            if (!m_documentLoader)
                return;
            if (m_documentLoader->isLoadingInAPISense() && !m_documentLoader->isStopping() && !m_checkingLoadCompleteForDetachment)
                return;

            setState(FrameStateComplete);

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            m_client->forceLayoutForNonHTML();
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if (m_frame.page()) {
                if (isBackForwardLoadType(m_loadType) || isReload(m_loadType))
                    history().restoreScrollPositionAndViewState();
            }

            if (m_stateMachine.creatingInitialEmptyDocument() || !m_stateMachine.committedFirstRealDocumentLoad())
                return;

            m_progressTracker->progressCompleted();
            Page* page = m_frame.page();
            if (page) {
                if (m_frame.isMainFrame()) {
                    tracePoint(MainResourceLoadDidEnd);
                    page->didFinishLoad();
                }
            }

            const ResourceError& error = m_documentLoader->mainDocumentError();

            AXObjectCache::AXLoadingEvent loadingEvent;
            if (!error.isNull()) {
                FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "checkLoadCompleteForThisFrame: Finished frame load with error (isTimeout = %d, isCancellation = %d, errorCode = %d)", error.isTimeout(), error.isCancellation(), error.errorCode());
                m_client->dispatchDidFailLoad(error);
                loadingEvent = AXObjectCache::AXLoadingFailed;
            } else {
                FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "checkLoadCompleteForThisFrame: Finished frame load");
#if ENABLE(DATA_DETECTION)
                auto document = m_frame.document();
                auto types = m_frame.settings().dataDetectorTypes();
                if (document && static_cast<uint32_t>(types)) {
                    m_frame.setDataDetectionResults(DataDetection::detectContentInRange(makeRangeSelectingNodeContents(*document), types, m_client->dataDetectionContext()));
                    if (m_frame.isMainFrame())
                        m_client->dispatchDidFinishDataDetection(m_frame.dataDetectionResults());
                }
#endif
                m_client->dispatchDidFinishLoad();
                loadingEvent = AXObjectCache::AXLoadingFinished;
            }

            // Notify accessibility.
            if (auto* document = m_frame.document()) {
                if (AXObjectCache* cache = document->existingAXObjectCache())
                    cache->frameLoadingEventNotification(&m_frame, loadingEvent);
            }

            // The above calls to dispatchDidFinishLoad() might have detached the Frame
            // from its Page and also might have caused Page to be deleted.
            // Don't assume 'page' is still available to use.
            if (m_frame.isMainFrame() && m_frame.page()) {
                ASSERT(&m_frame.page()->mainFrame() == &m_frame);
                m_frame.page()->diagnosticLoggingClient().logDiagnosticMessageWithResult(DiagnosticLoggingKeys::pageLoadedKey(), emptyString(), error.isNull() ? DiagnosticLoggingResultPass : DiagnosticLoggingResultFail, ShouldSample::Yes);
            }

            return;
        }
        
        case FrameStateComplete:
            m_loadType = FrameLoadType::Standard;
            frameLoadCompleted();
            return;
    }

    ASSERT_NOT_REACHED();
}

void FrameLoader::setOriginalURLForDownloadRequest(ResourceRequest& request)
{
    // FIXME: Rename firstPartyForCookies back to mainDocumentURL. It was a mistake to think that it was only used for cookies.
    // The originalURL is defined as the URL of the page where the download was initiated.
    URL originalURL;
    auto* initiator = m_frame.document();
    if (initiator) {
        originalURL = initiator->firstPartyForCookies();
        // If there is no main document URL, it means that this document is newly opened and just for download purpose.
        // In this case, we need to set the originalURL to this document's opener's main document URL.
        if (originalURL.isEmpty() && opener() && opener()->document()) {
            originalURL = opener()->document()->firstPartyForCookies();
            initiator = opener()->document();
        }
    }
    // If the originalURL is the same as the requested URL, we are processing a download
    // initiated directly without a page and do not need to specify the originalURL.
    if (originalURL == request.url())
        request.setFirstPartyForCookies(URL());
    else
        request.setFirstPartyForCookies(originalURL);
    addSameSiteInfoToRequestIfNeeded(request, initiator);
}

void FrameLoader::didReachLayoutMilestone(OptionSet<LayoutMilestone> milestones)
{
    ASSERT(m_frame.isMainFrame());

    m_client->dispatchDidReachLayoutMilestone(milestones);
}

void FrameLoader::didFirstLayout()
{
#if PLATFORM(IOS_FAMILY)
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

void FrameLoader::didReachVisuallyNonEmptyState()
{
    ASSERT(m_frame.isMainFrame());
    m_client->dispatchDidReachVisuallyNonEmptyState();
}

void FrameLoader::frameLoadCompleted()
{
    // Note: Can be called multiple times.

    m_client->frameLoadCompleted();

    history().updateForFrameLoadCompleted();

    // After a canceled provisional load, firstLayoutDone is false.
    // Reset it to true if we're displaying a page.
    if (m_documentLoader && m_stateMachine.committedFirstRealDocumentLoad() && !m_stateMachine.isDisplayingInitialEmptyDocument() && !m_stateMachine.firstLayoutDone())
        m_stateMachine.advanceTo(FrameLoaderStateMachine::FirstLayoutDone);
}

void FrameLoader::detachChildren()
{
    // detachChildren() will fire the unload event in each subframe and the
    // HTML specification states that the parent document's ignore-opens-during-unload counter while
    // this event is being fired in its subframes:
    // https://html.spec.whatwg.org/multipage/browsers.html#unload-a-document
    IgnoreOpensDuringUnloadCountIncrementer ignoreOpensDuringUnloadCountIncrementer(m_frame.document());

    // detachChildren() will fire the unload event in each subframe and the
    // HTML specification states that navigations should be prevented during the prompt to unload algorithm:
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#navigate
    std::unique_ptr<NavigationDisabler> navigationDisabler;
    if (m_frame.isMainFrame())
        navigationDisabler = makeUnique<NavigationDisabler>(&m_frame);

    // Any subframe inserted by unload event handlers executed in the loop below will not get unloaded
    // because we create a copy of the subframes list before looping. Therefore, it would be unsafe to
    // allow loading of subframes at this point.
    SubframeLoadingDisabler subframeLoadingDisabler(m_frame.document());

    Vector<Ref<Frame>, 16> childrenToDetach;
    childrenToDetach.reserveInitialCapacity(m_frame.tree().childCount());
    for (Frame* child = m_frame.tree().lastChild(); child; child = child->tree().previousSibling())
        childrenToDetach.uncheckedAppend(*child);
    for (auto& child : childrenToDetach)
        child->loader().detachFromParent();
}

void FrameLoader::closeAndRemoveChild(Frame& child)
{
    child.tree().detachFromParent();

    child.setView(nullptr);
    child.willDetachPage();
    child.detachFromPage();

    m_frame.tree().removeChild(child);
}

// Called every time a resource is completely loaded or an error is received.
void FrameLoader::checkLoadComplete()
{
    m_shouldCallCheckLoadComplete = false;

    if (!m_frame.page())
        return;

    ASSERT(m_client->hasWebView());
    
    // FIXME: Always traversing the entire frame tree is a bit inefficient, but 
    // is currently needed in order to null out the previous history item for all frames.
    Vector<Ref<Frame>, 16> frames;
    for (Frame* frame = &m_frame.mainFrame(); frame; frame = frame->tree().traverseNext())
        frames.append(*frame);

    // To process children before their parents, iterate the vector backwards.
    for (auto frame = frames.rbegin(); frame != frames.rend(); ++frame) {
        if ((*frame)->page())
            (*frame)->loader().checkLoadCompleteForThisFrame();
    }
}

int FrameLoader::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return m_frame.document()->cachedResourceLoader().requestCount();

    int count = 0;
    for (Frame* frame = &m_frame; frame; frame = frame->tree().traverseNext(&m_frame))
        count += frame->document()->cachedResourceLoader().requestCount();
    return count;
}

String FrameLoader::userAgent(const URL& url) const
{
    String userAgent;

    if (auto* documentLoader = m_frame.mainFrame().loader().activeDocumentLoader()) {
        if (m_frame.settings().needsSiteSpecificQuirks())
            userAgent = documentLoader->customUserAgentAsSiteSpecificQuirks();
        if (userAgent.isEmpty())
            userAgent = documentLoader->customUserAgent();
    }

    InspectorInstrumentation::applyUserAgentOverride(m_frame, userAgent);

    if (!userAgent.isEmpty())
        return userAgent;

    return m_client->userAgent(url);
}

String FrameLoader::navigatorPlatform() const
{
    if (auto* documentLoader = m_frame.mainFrame().loader().activeDocumentLoader()) {
        auto& customNavigatorPlatform = documentLoader->customNavigatorPlatform();
        if (!customNavigatorPlatform.isEmpty())
            return customNavigatorPlatform;
    }
    return String();
}

void FrameLoader::dispatchOnloadEvents()
{
    m_client->dispatchDidDispatchOnloadEvents();

    if (documentLoader())
        documentLoader()->dispatchOnloadEvents();
}

void FrameLoader::frameDetached()
{
    // Calling stopAllLoadersAndCheckCompleteness() can cause the frame to be deallocated, including the frame loader.
    Ref<Frame> protectedFrame(m_frame);

    if (m_checkTimer.isActive()) {
        m_checkTimer.stop();
        checkCompletenessNow();
    }

    if (m_frame.document()->backForwardCacheState() != Document::InBackForwardCache) {
        stopAllLoadersAndCheckCompleteness();
        m_frame.document()->stopActiveDOMObjects();
    }

    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    // Calling stopAllLoaders() can cause the frame to be deallocated, including the frame loader.
    Ref<Frame> protect(m_frame);

    closeURL();
    history().saveScrollPositionAndViewStateToItem(history().currentItem());
    detachChildren();
    if (m_frame.document()->backForwardCacheState() != Document::InBackForwardCache) {
        // stopAllLoaders() needs to be called after detachChildren() if the document is not in the back/forward cache,
        // because detachedChildren() will trigger the unload event handlers of any child frames, and those event
        // handlers might start a new subresource load in this frame.
        stopAllLoaders(ShouldClearProvisionalItem, StopLoadingPolicy::AlwaysStopLoading);
    }

    InspectorInstrumentation::frameDetachedFromParent(m_frame);

    detachViewsAndDocumentLoader();

    m_progressTracker = nullptr;

    if (Frame* parent = m_frame.tree().parent()) {
        parent->loader().closeAndRemoveChild(m_frame);
        parent->loader().scheduleCheckCompleted();
        parent->loader().scheduleCheckLoadComplete();
    } else {
        m_frame.setView(nullptr);
        m_frame.willDetachPage();
        m_frame.detachFromPage();
    }
}

void FrameLoader::detachViewsAndDocumentLoader()
{
    m_client->detachedFromParent2();
    setDocumentLoader(nullptr);
    m_client->detachedFromParent3();
}

ResourceRequestCachePolicy FrameLoader::defaultRequestCachingPolicy(const ResourceRequest& request, FrameLoadType loadType, bool isMainResource)
{
    if (m_overrideCachePolicyForTesting)
        return m_overrideCachePolicyForTesting.value();

    if (isMainResource) {
        if (isReload(loadType) || request.isConditional())
            return ResourceRequestCachePolicy::ReloadIgnoringCacheData;

        return ResourceRequestCachePolicy::UseProtocolCachePolicy;
    }

    if (request.isConditional())
        return ResourceRequestCachePolicy::ReloadIgnoringCacheData;

    if (documentLoader()->isLoadingInAPISense()) {
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
        return (mainDocumentOriginalCachePolicy == ResourceRequestCachePolicy::ReturnCacheDataDontLoad) ? ResourceRequestCachePolicy::ReturnCacheDataElseLoad : mainDocumentOriginalCachePolicy;
    }

    return ResourceRequestCachePolicy::UseProtocolCachePolicy;
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request, IsMainResource mainResource, FrameLoadType loadType)
{
    // If the request came from a previous process due to process-swap-on-navigation then we should not modify the request.
    if (m_currentLoadContinuingState == LoadContinuingState::ContinuingWithRequest)
        return;

    // Don't set the cookie policy URL if it's already been set.
    // But make sure to set it on all requests regardless of protocol, as it has significance beyond the cookie policy (<rdar://problem/6616664>).
    bool isMainResource = mainResource == IsMainResource::Yes;
    bool isMainFrameMainResource = isMainResource && m_frame.isMainFrame();
    if (request.firstPartyForCookies().isEmpty()) {
        if (isMainFrameMainResource)
            request.setFirstPartyForCookies(request.url());
        else if (Document* document = m_frame.document())
            request.setFirstPartyForCookies(document->firstPartyForCookies());
    }

    if (request.isSameSiteUnspecified()) {
        auto* initiator = m_frame.document();
        if (isMainResource) {
            auto* ownerFrame = m_frame.tree().parent();
            if (!ownerFrame && m_stateMachine.isDisplayingInitialEmptyDocument())
                ownerFrame = m_opener;
            if (ownerFrame)
                initiator = ownerFrame->document();
            ASSERT(ownerFrame || m_frame.isMainFrame());
        }
        addSameSiteInfoToRequestIfNeeded(request, initiator);
    }
    request.setIsTopSite(isMainFrameMainResource);

    Page* page = frame().page();
    bool hasSpecificCachePolicy = request.cachePolicy() != ResourceRequestCachePolicy::UseProtocolCachePolicy;

    if (page && page->isResourceCachingDisabledByWebInspector()) {
        request.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);
        loadType = FrameLoadType::ReloadFromOrigin;
    } else if (!hasSpecificCachePolicy)
        request.setCachePolicy(defaultRequestCachingPolicy(request, loadType, isMainResource));

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolIsInHTTPFamily())
        return;

    if (!hasSpecificCachePolicy && request.cachePolicy() == ResourceRequestCachePolicy::ReloadIgnoringCacheData) {
        if (loadType == FrameLoadType::Reload)
            request.setHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::maxAge0());
        else if (loadType == FrameLoadType::ReloadFromOrigin) {
            request.setHTTPHeaderField(HTTPHeaderName::CacheControl, HTTPHeaderValues::noCache());
            request.setHTTPHeaderField(HTTPHeaderName::Pragma, HTTPHeaderValues::noCache());
        }
    }

    if (m_overrideResourceLoadPriorityForTesting)
        request.setPriority(m_overrideResourceLoadPriorityForTesting.value());

    // Make sure we send the Origin header.
    addHTTPOriginIfNeeded(request, String());

    applyUserAgentIfNeeded(request);

    if (isMainResource)
        request.setHTTPHeaderField(HTTPHeaderName::Accept, CachedResourceRequest::acceptHeaderValueFromType(CachedResource::Type::MainResource));

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

    // FIXME: take referrer-policy into account.
    // https://bugs.webkit.org/show_bug.cgi?id=209066

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

// Implements the "'Same-site' and 'cross-site' Requests" algorithm from <https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-00#section-2.1>.
// The algorithm is ammended to treat URLs that inherit their security origin from their owner (e.g. about:blank)
// as same-site. This matches the behavior of Chrome and Firefox.
void FrameLoader::addSameSiteInfoToRequestIfNeeded(ResourceRequest& request, const Document* initiator)
{
    if (!request.isSameSiteUnspecified())
        return;
    if (!initiator) {
        request.setIsSameSite(true);
        return;
    }
    if (SecurityPolicy::shouldInheritSecurityOriginFromOwner(request.url())) {
        request.setIsSameSite(true);
        return;
    }
    request.setIsSameSite(areRegistrableDomainsEqual(initiator->siteForCookies(), request.url()));
}

void FrameLoader::loadPostRequest(FrameLoadRequest&& request, const String& referrer, FrameLoadType loadType, Event* event, RefPtr<FormState>&& formState, CompletionHandler<void()>&& completionHandler)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadPostRequest: frame load started");

    String frameName = request.frameName();
    LockHistory lockHistory = request.lockHistory();
    AllowNavigationToInvalidURL allowNavigationToInvalidURL = request.allowNavigationToInvalidURL();
    NewFrameOpenerPolicy openerPolicy = request.newFrameOpenerPolicy();

    const ResourceRequest& inRequest = request.resourceRequest();
    const URL& url = inRequest.url();
    const String& contentType = inRequest.httpContentType();
    String origin = inRequest.httpOrigin();

    ResourceRequest workingResourceRequest(url);    

    if (!referrer.isEmpty())
        workingResourceRequest.setHTTPReferrer(referrer);
    workingResourceRequest.setHTTPOrigin(origin);
    workingResourceRequest.setHTTPMethod("POST");
    workingResourceRequest.setHTTPBody(inRequest.httpBody());
    workingResourceRequest.setHTTPContentType(contentType);
    addExtraFieldsToRequest(workingResourceRequest, IsMainResource::Yes, loadType);

    if (Document* document = m_frame.document())
        document->contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(workingResourceRequest, ContentSecurityPolicy::InsecureRequestType::Load);

    NavigationAction action { request.requester(), workingResourceRequest, request.initiatedByMainFrame(), loadType, true, event, request.shouldOpenExternalURLsPolicy(), { } };
    action.setLockHistory(lockHistory);

    if (!frameName.isEmpty()) {
        // The search for a target frame is done earlier in the case of form submission.
        if (auto* targetFrame = formState ? nullptr : findFrameForNavigation(frameName)) {
            targetFrame->loader().loadWithNavigationAction(workingResourceRequest, WTFMove(action), loadType, WTFMove(formState), allowNavigationToInvalidURL, WTFMove(completionHandler));
            return;
        }

        policyChecker().checkNewWindowPolicy(WTFMove(action), WTFMove(workingResourceRequest), WTFMove(formState), frameName, [this, allowNavigationToInvalidURL, openerPolicy, completionHandler = WTFMove(completionHandler)] (const ResourceRequest& request, WeakPtr<FormState>&& formState, const String& frameName, const NavigationAction& action, ShouldContinuePolicyCheck shouldContinue) mutable {
            continueLoadAfterNewWindowPolicy(request, formState.get(), frameName, action, shouldContinue, allowNavigationToInvalidURL, openerPolicy);
            completionHandler();
        });
        return;
    }

    // must grab this now, since this load may stop the previous load and clear this flag
    bool isRedirect = m_quickRedirectComing;
    loadWithNavigationAction(workingResourceRequest, WTFMove(action), loadType, WTFMove(formState), allowNavigationToInvalidURL, [this, isRedirect, protectedFrame = makeRef(m_frame), completionHandler = WTFMove(completionHandler)] () mutable {
        if (isRedirect) {
            m_quickRedirectComing = false;
            if (m_provisionalDocumentLoader)
                m_provisionalDocumentLoader->setIsClientRedirect(true);
            else if (m_policyDocumentLoader)
                m_policyDocumentLoader->setIsClientRedirect(true);
        }
        completionHandler();
    });
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, ClientCredentialPolicy clientCredentialPolicy, const FetchOptions& options, const HTTPHeaderMap& originalRequestHeaders, ResourceError& error, ResourceResponse& response, RefPtr<SharedBuffer>& data)
{
    ASSERT(m_frame.document());
    String referrer = SecurityPolicy::generateReferrerHeader(m_frame.document()->referrerPolicy(), request.url(), outgoingReferrer());
    
    ResourceRequest initialRequest = request;
    initialRequest.setTimeoutInterval(10);
    
    if (!referrer.isEmpty())
        initialRequest.setHTTPReferrer(referrer);
    addHTTPOriginIfNeeded(initialRequest, outgoingOrigin());

    initialRequest.setFirstPartyForCookies(m_frame.mainFrame().loader().documentLoader()->request().url());
    
    addExtraFieldsToRequest(initialRequest, IsMainResource::No);

    applyUserAgentIfNeeded(initialRequest);

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

#if ENABLE(CONTENT_EXTENSIONS)
    if (error.isNull()) {
        if (auto* page = m_frame.page()) {
            if (m_documentLoader) {
                auto results = page->userContentProvider().processContentRuleListsForLoad(newRequest.url(), ContentExtensions::ResourceType::Raw, *m_documentLoader);
                bool blockedLoad = results.summary.blockedLoad;
                ContentExtensions::applyResultsToRequest(WTFMove(results), page, newRequest);
                if (blockedLoad) {
                    newRequest = { };
                    error = ResourceError(errorDomainWebKitInternal, 0, initialRequest.url(), emptyString());
                    response = { };
                    data = nullptr;
                }
            }
        }
    }
#endif

    m_frame.document()->contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(newRequest, ContentSecurityPolicy::InsecureRequestType::Load);
    
    if (error.isNull()) {
        ASSERT(!newRequest.isNull());

        if (!documentLoader()->applicationCacheHost().maybeLoadSynchronously(newRequest, error, response, data)) {
            Vector<char> buffer;
            platformStrategies()->loaderStrategy()->loadResourceSynchronously(*this, identifier, newRequest, clientCredentialPolicy, options, originalRequestHeaders, error, response, buffer);
            data = SharedBuffer::create(WTFMove(buffer));
            documentLoader()->applicationCacheHost().maybeLoadFallbackSynchronously(newRequest, error, response, data);
            ResourceLoadObserver::shared().logSubresourceLoading(&m_frame, newRequest, response,
                (isScriptLikeDestination(options.destination) ? ResourceLoadObserver::FetchDestinationIsScriptLike::Yes : ResourceLoadObserver::FetchDestinationIsScriptLike::No));
        }
    }
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, request, response, data ? data->data() : nullptr, data ? data->size() : 0, -1, error);
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
    if (m_client->shouldFallBack(error)) {
        HTMLFrameOwnerElement* owner = m_frame.ownerElement();
        if (is<HTMLObjectElement>(owner))
            downcast<HTMLObjectElement>(*owner).renderFallbackContent();
    }

    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        if (m_submittedFormURL == m_provisionalDocumentLoader->originalRequestCopy().url())
            m_submittedFormURL = URL();
            
        // We might have made a back/forward cache item, but now we're bailing out due to an error before we ever
        // transitioned to the new page (before WebFrameState == commit).  The goal here is to restore any state
        // so that the existing view (that wenever got far enough to replace) can continue being used.
        history().invalidateCurrentItemCachedPage();
        
        // Call clientRedirectCancelledOrFinished here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect. The frame load delegate may have saved some state about
        // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:. Since we are definitely
        // not going to use this provisional resource, as it was cancelled, notify the frame load delegate that the redirect
        // has ended.
        if (m_sentRedirectNotification)
            clientRedirectCancelledOrFinished(NewLoadInProgress::No);
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

    // Calling stopLoading() on the provisional document loader can cause the underlying
    // frame to be deallocated.
    Ref<Frame> protectedFrame(m_frame);

    // If we have a provisional request for a different document, a fragment scroll should cancel it.
    if (m_provisionalDocumentLoader && !equalIgnoringFragmentIdentifier(m_provisionalDocumentLoader->request().url(), request.url())) {
        m_provisionalDocumentLoader->stopLoading();
        FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueFragmentScrollAfterNavigationPolicy: Clearing provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
        setProvisionalDocumentLoader(nullptr);
    }

    bool isRedirect = m_quickRedirectComing || policyChecker().loadType() == FrameLoadType::RedirectWithLockedBackForwardList;
    loadInSameDocument(request.url(), 0, !isRedirect);
}

bool FrameLoader::shouldPerformFragmentNavigation(bool isFormSubmission, const String& httpMethod, FrameLoadType loadType, const URL& url)
{
    // We don't do this if we are submitting a form with method other than "GET", explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    // These rules were originally based on what KHTML was doing in KHTMLPart::openURL.

    // FIXME: What about load types other than Standard and Reload?

    return (!isFormSubmission || equalLettersIgnoringASCIICase(httpMethod, "get"))
        && !isReload(loadType)
        && loadType != FrameLoadType::Same
        && m_frame.document()->backForwardCacheState() != Document::InBackForwardCache
        && !shouldReload(m_frame.document()->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame.document()->isFrameSet();
}

static bool itemAllowsScrollRestoration(HistoryItem* historyItem)
{
    return !historyItem || historyItem->shouldRestoreScrollPosition();
}

static bool isSameDocumentReload(bool isNewNavigation, FrameLoadType loadType)
{
    return !isNewNavigation && !isBackForwardLoadType(loadType);
}

void FrameLoader::scrollToFragmentWithParentBoundary(const URL& url, bool isNewNavigation)
{
    auto view = makeRefPtr(m_frame.view());
    auto document = makeRefPtr(m_frame.document());
    if (!view || !document)
        return;

    if (isSameDocumentReload(isNewNavigation, m_loadType) || itemAllowsScrollRestoration(history().currentItem())) {
        // https://html.spec.whatwg.org/multipage/browsing-the-web.html#try-to-scroll-to-the-fragment
        if (!document->haveStylesheetsLoaded())
            document->setGotoAnchorNeededAfterStylesheetsLoad(true);
        else
            view->scrollToFragment(url);
    }

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
        NavigationDisabler navigationDisabler(&m_frame);
        IgnoreOpensDuringUnloadCountIncrementer ignoreOpensDuringUnloadCountIncrementer(m_frame.document());
        size_t i;

        for (i = 0; i < targetFrames.size(); i++) {
            if (!targetFrames[i]->tree().isDescendantOf(&m_frame))
                continue;
            if (!targetFrames[i]->loader().dispatchBeforeUnloadEvent(page->chrome(), this))
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

void FrameLoader::dispatchUnloadEvents(UnloadEventPolicy unloadEventPolicy)
{
    if (!m_frame.document())
        return;

    if (m_pageDismissalEventBeingDispatched != PageDismissalType::None)
        return;

    // We store the frame's page in a local variable because the frame might get detached inside dispatchEvent.
    ForbidPromptsScope forbidPrompts(m_frame.page());
    ForbidSynchronousLoadsScope forbidSynchronousLoads(m_frame.page());
    IgnoreOpensDuringUnloadCountIncrementer ignoreOpensDuringUnloadCountIncrementer(m_frame.document());

    if (m_didCallImplicitClose && !m_wasUnloadEventEmitted) {
        auto* currentFocusedElement = m_frame.document()->focusedElement();
        if (is<HTMLInputElement>(currentFocusedElement))
            downcast<HTMLInputElement>(*currentFocusedElement).endEditing();
        if (m_pageDismissalEventBeingDispatched == PageDismissalType::None) {
            if (unloadEventPolicy == UnloadEventPolicyUnloadAndPageHide) {
                m_pageDismissalEventBeingDispatched = PageDismissalType::PageHide;
                m_frame.document()->domWindow()->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, m_frame.document()->backForwardCacheState() == Document::AboutToEnterBackForwardCache), m_frame.document());
            }

            // FIXME: update Page Visibility state here.
            // https://bugs.webkit.org/show_bug.cgi?id=116770

            if (m_frame.document()->backForwardCacheState() == Document::NotInBackForwardCache) {
                Ref<Event> unloadEvent(Event::create(eventNames().unloadEvent, Event::CanBubble::No, Event::IsCancelable::No));
                // The DocumentLoader (and thus its LoadTiming) might get destroyed
                // while dispatching the event, so protect it to prevent writing the end
                // time into freed memory.
                RefPtr<DocumentLoader> documentLoader = m_provisionalDocumentLoader;
                m_pageDismissalEventBeingDispatched = PageDismissalType::Unload;
                if (documentLoader && documentLoader->timing().startTime() && !documentLoader->timing().unloadEventStart() && !documentLoader->timing().unloadEventEnd()) {
                    auto& timing = documentLoader->timing();
                    timing.markUnloadEventStart();
                    m_frame.document()->domWindow()->dispatchEvent(unloadEvent, m_frame.document());
                    timing.markUnloadEventEnd();
                } else
                    m_frame.document()->domWindow()->dispatchEvent(unloadEvent, m_frame.document());
            }
        }
        m_pageDismissalEventBeingDispatched = PageDismissalType::None;
        m_wasUnloadEventEmitted = true;
    }

    // Dispatching the unload event could have made m_frame.document() null.
    if (!m_frame.document())
        return;

    if (m_frame.document()->backForwardCacheState() != Document::NotInBackForwardCache)
        return;

    // Don't remove event listeners from a transitional empty document (see bug 28716 for more information).
    bool keepEventListeners = m_stateMachine.isDisplayingInitialEmptyDocument() && m_provisionalDocumentLoader
        && m_frame.document()->isSecureTransitionTo(m_provisionalDocumentLoader->url());

    if (!keepEventListeners)
        m_frame.document()->removeAllEventListeners();
}

static bool shouldAskForNavigationConfirmation(Document& document, const BeforeUnloadEvent& event)
{
    // Confirmation dialog should not be displayed when the allow-modals flag is not set.
    if (document.isSandboxed(SandboxModals))
        return false;

    bool userDidInteractWithPage = document.topDocument().userDidInteractWithPage();
    // Web pages can request we ask for confirmation before navigating by:
    // - Cancelling the BeforeUnloadEvent (modern way)
    // - Setting the returnValue attribute on the BeforeUnloadEvent to a non-empty string.
    // - Returning a non-empty string from the event handler, which is then set as returnValue
    //   attribute on the BeforeUnloadEvent.
    return userDidInteractWithPage && (event.defaultPrevented() || !event.returnValue().isEmpty());
}

bool FrameLoader::dispatchBeforeUnloadEvent(Chrome& chrome, FrameLoader* frameLoaderBeingNavigated)
{
    DOMWindow* domWindow = m_frame.document()->domWindow();
    if (!domWindow)
        return true;

    RefPtr<Document> document = m_frame.document();
    if (!document->bodyOrFrameset())
        return true;
    
    Ref<BeforeUnloadEvent> beforeUnloadEvent = BeforeUnloadEvent::create();

    {
        SetForScope<PageDismissalType> change(m_pageDismissalEventBeingDispatched, PageDismissalType::BeforeUnload);
        ForbidPromptsScope forbidPrompts(m_frame.page());
        ForbidSynchronousLoadsScope forbidSynchronousLoads(m_frame.page());
        domWindow->dispatchEvent(beforeUnloadEvent, domWindow->document());
    }

    if (!beforeUnloadEvent->defaultPrevented())
        document->defaultEventHandler(beforeUnloadEvent.get());

    if (!shouldAskForNavigationConfirmation(*document, beforeUnloadEvent))
        return true;

    // If the navigating FrameLoader has already shown a beforeunload confirmation panel for the current navigation attempt,
    // this frame is not allowed to cause another one to be shown.
    if (frameLoaderBeingNavigated->m_currentNavigationHasShownBeforeUnloadConfirmPanel) {
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Blocked attempt to show multiple beforeunload confirmation dialogs for the same navigation."_s);
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
            if (!m_frame.document() || !m_frame.document()->securityOrigin().canAccess(parentDocument->securityOrigin())) {
                document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Blocked attempt to show beforeunload confirmation dialog on behalf of a frame with different security origin. Protocols, domains, and ports must match."_s);
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
    return chrome.runBeforeUnloadConfirmPanel(text, m_frame);
}

void FrameLoader::continueLoadAfterNavigationPolicy(const ResourceRequest& request, FormState* formState, NavigationPolicyDecision navigationPolicyDecision, AllowNavigationToInvalidURL allowNavigationToInvalidURL)
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT(m_policyDocumentLoader || !m_provisionalDocumentLoader->unreachableURL().isEmpty());

    bool isTargetItem = history().provisionalItem() ? history().provisionalItem()->isTargetItem() : false;

    bool urlIsDisallowed = allowNavigationToInvalidURL == AllowNavigationToInvalidURL::No && !request.url().isValid();
    bool canContinue = navigationPolicyDecision == NavigationPolicyDecision::ContinueLoad && shouldClose() && !urlIsDisallowed;

    if (!canContinue) {
        FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy: can't continue loading frame due to the following reasons ("
            "allowNavigationToInvalidURL = %d, "
            "requestURLIsValid = %d, "
            "navigationPolicyDecision = %d)",
            static_cast<int>(allowNavigationToInvalidURL),
            request.url().isValid(),
            static_cast<int>(navigationPolicyDecision));

        // If we were waiting for a quick redirect, but the policy delegate decided to ignore it, then we 
        // need to report that the client redirect was cancelled.
        // FIXME: The client should be told about ignored non-quick redirects, too.
        if (m_quickRedirectComing)
            clientRedirectCancelledOrFinished(NewLoadInProgress::No);

        if (navigationPolicyDecision == NavigationPolicyDecision::StopAllLoads) {
            stopAllLoaders();
            m_checkTimer.stop();
        }

        setPolicyDocumentLoader(nullptr);
        checkCompleted();

        if (navigationPolicyDecision != NavigationPolicyDecision::StopAllLoads)
            checkLoadComplete();

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back. For sanity,
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || m_frame.isMainFrame()) && isBackForwardLoadType(policyChecker().loadType())) {
            if (Page* page = m_frame.page()) {
                if (HistoryItem* resetItem = m_frame.mainFrame().loader().history().currentItem())
                    page->backForward().setCurrentItem(*resetItem);
            }
        }
        return;
    }

    FrameLoadType type = policyChecker().loadType();
    // A new navigation is in progress, so don't clear the history's provisional item.
    stopAllLoaders(ShouldNotClearProvisionalItem);
    
    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load. 
    if (!m_frame.page()) {
        FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy: can't continue loading frame because it became defunct");
        return;
    }

    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy: Setting provisional document loader (m_provisionalDocumentLoader=%p)", m_provisionalDocumentLoader.get());
    m_loadType = type;
    setState(FrameStateProvisional);

    setPolicyDocumentLoader(nullptr);

    if (isBackForwardLoadType(type)) {
        auto& diagnosticLoggingClient = m_frame.page()->diagnosticLoggingClient();
        if (history().provisionalItem() && history().provisionalItem()->isInBackForwardCache()) {
            diagnosticLoggingClient.logDiagnosticMessageWithResult(DiagnosticLoggingKeys::backForwardCacheKey(), DiagnosticLoggingKeys::retrievalKey(), DiagnosticLoggingResultPass, ShouldSample::Yes);
            loadProvisionalItemFromCachedPage();
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy: can't continue loading frame because it will be loaded from cache");
            return;
        }
        diagnosticLoggingClient.logDiagnosticMessageWithResult(DiagnosticLoggingKeys::backForwardCacheKey(), DiagnosticLoggingKeys::retrievalKey(), DiagnosticLoggingResultFail, ShouldSample::Yes);
    }

    CompletionHandler<void()> completionHandler = [this, protectedFrame = makeRef(m_frame)] () mutable {
        if (!m_provisionalDocumentLoader) {
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy (completionHandler): Frame load canceled - no provisional document loader before prepareForLoadStart");
            return;
        }
        
        prepareForLoadStart();

        // The load might be cancelled inside of prepareForLoadStart(), nulling out the m_provisionalDocumentLoader,
        // so we need to null check it again.
        if (!m_provisionalDocumentLoader) {
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy (completionHandler): Frame load canceled - no provisional document loader after prepareForLoadStart");
            return;
        }
        
        DocumentLoader* activeDocLoader = activeDocumentLoader();
        if (activeDocLoader && activeDocLoader->isLoadingMainResource()) {
            FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "continueLoadAfterNavigationPolicy (completionHandler): Main frame already being loaded");
            return;
        }
        
        m_loadingFromCachedPage = false;

        m_provisionalDocumentLoader->startLoadingMainResource();
    };
    
    if (!formState) {
        completionHandler();
        return;
    }

    m_client->dispatchWillSubmitForm(*formState, WTFMove(completionHandler));
}

void FrameLoader::continueLoadAfterNewWindowPolicy(const ResourceRequest& request,
    FormState* formState, const String& frameName, const NavigationAction& action, ShouldContinuePolicyCheck shouldContinue, AllowNavigationToInvalidURL allowNavigationToInvalidURL, NewFrameOpenerPolicy openerPolicy)
{
    if (shouldContinue != ShouldContinuePolicyCheck::Yes)
        return;

    Ref<Frame> frame(m_frame);
    RefPtr<Frame> mainFrame = m_client->dispatchCreatePage(action);
    if (!mainFrame)
        return;

    SandboxFlags sandboxFlags = frame->loader().effectiveSandboxFlags();
    if (sandboxFlags & SandboxPropagatesToAuxiliaryBrowsingContexts)
        mainFrame->loader().forceSandboxFlags(sandboxFlags);

    if (!equalIgnoringASCIICase(frameName, "_blank"))
        mainFrame->tree().setName(frameName);

    mainFrame->page()->setOpenedByDOM();
    mainFrame->loader().m_client->dispatchShow();
    if (openerPolicy == NewFrameOpenerPolicy::Allow) {
        mainFrame->loader().setOpener(frame.ptr());
        mainFrame->document()->setReferrerPolicy(frame->document()->referrerPolicy());
    }

    NavigationAction newAction { *frame->document(), request, InitiatedByMainFrame::Unknown, NavigationType::Other, action.shouldOpenExternalURLsPolicy(), nullptr, action.downloadAttribute() };
    mainFrame->loader().loadWithNavigationAction(request, WTFMove(newAction), FrameLoadType::Standard, formState, allowNavigationToInvalidURL);
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

void FrameLoader::loadedResourceFromMemoryCache(CachedResource& resource, ResourceRequest& newRequest, ResourceError& error)
{
    Page* page = m_frame.page();
    if (!page)
        return;

    if (!resource.shouldSendResourceLoadCallbacks() || m_documentLoader->haveToldClientAboutLoad(resource.url().string()))
        return;

    // Main resource delegate messages are synthesized in MainResourceLoader, so we must not send them here.
    if (resource.type() == CachedResource::Type::MainResource)
        return;

    if (!page->areMemoryCacheClientCallsEnabled()) {
        InspectorInstrumentation::didLoadResourceFromMemoryCache(*page, m_documentLoader.get(), &resource);
        m_documentLoader->recordMemoryCacheLoadForFutureClientNotification(resource.resourceRequest());
        m_documentLoader->didTellClientAboutLoad(resource.url().string());
        return;
    }

    if (m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), newRequest, resource.response(), resource.encodedSize())) {
        InspectorInstrumentation::didLoadResourceFromMemoryCache(*page, m_documentLoader.get(), &resource);
        m_documentLoader->didTellClientAboutLoad(resource.url().string());
        return;
    }

    unsigned long identifier;
    requestFromDelegate(newRequest, identifier, error);

    ResourceResponse response = resource.response();
    response.setSource(ResourceResponse::Source::MemoryCache);
    notifier().sendRemainingDelegateMessages(m_documentLoader.get(), identifier, newRequest, response, 0, resource.encodedSize(), 0, error);
}

void FrameLoader::applyUserAgentIfNeeded(ResourceRequest& request)
{
    if (!request.hasHTTPHeaderField(HTTPHeaderName::UserAgent)) {
        String userAgent = this->userAgent(request.url());
        ASSERT(!userAgent.isNull());
        request.setHTTPUserAgent(userAgent);
    }
}

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const URL& url, unsigned long requestIdentifier)
{
    Frame& topFrame = m_frame.tree().top();
    if (&m_frame == &topFrame)
        return false;

    XFrameOptionsDisposition disposition = parseXFrameOptionsHeader(content);

    switch (disposition) {
    case XFrameOptionsDisposition::SameOrigin: {
        auto origin = SecurityOrigin::create(url);
        if (!origin->isSameSchemeHostPort(topFrame.document()->securityOrigin()))
            return true;
        for (Frame* frame = m_frame.tree().parent(); frame; frame = frame->tree().parent()) {
            if (!origin->isSameSchemeHostPort(frame->document()->securityOrigin()))
                return true;
        }
        return false;
    }
    case XFrameOptionsDisposition::Deny:
        return true;
    case XFrameOptionsDisposition::AllowAll:
        return false;
    case XFrameOptionsDisposition::Conflict:
        m_frame.document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Multiple 'X-Frame-Options' headers with conflicting values ('" + content + "') encountered when loading '" + url.stringCenterEllipsizedToLength() + "'. Falling back to 'DENY'.", requestIdentifier);
        return true;
    case XFrameOptionsDisposition::Invalid:
        m_frame.document()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "Invalid 'X-Frame-Options' header encountered when loading '" + url.stringCenterEllipsizedToLength() + "': '" + content + "' is not a recognized directive. The header will be ignored.", requestIdentifier);
        return false;
    case XFrameOptionsDisposition::None:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void FrameLoader::loadProvisionalItemFromCachedPage()
{
    DocumentLoader* provisionalLoader = provisionalDocumentLoader();
    LOG(BackForwardCache, "FrameLoader::loadProvisionalItemFromCachedPage Loading provisional DocumentLoader %p with URL '%s' from CachedPage", provisionalDocumentLoader(), provisionalDocumentLoader()->url().stringCenterEllipsizedToLength().utf8().data());

    prepareForLoadStart();

    m_loadingFromCachedPage = true;
    
    // Should have timing data from previous time(s) the page was shown.
    ASSERT(provisionalLoader->timing().startTime());
    provisionalLoader->resetTiming();
    provisionalLoader->timing().markStartTime();
    
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
    if (!url.isAboutSrcDoc())
        return false;
    HTMLFrameOwnerElement* ownerElement = m_frame.ownerElement();
    if (!ownerElement)
        return false;
    if (!ownerElement->hasTagName(iframeTag))
        return false;
    return ownerElement->hasAttributeWithoutSynchronization(srcdocAttr);
}

Frame* FrameLoader::findFrameForNavigation(const AtomString& name, Document* activeDocument)
{
    // FIXME: Eventually all callers should supply the actual activeDocument so we can call canNavigate with the right document.
    if (!activeDocument)
        activeDocument = m_frame.document();

    auto* frame = m_frame.tree().find(name, activeDocument->frame() ? *activeDocument->frame() : m_frame);

    if (!activeDocument->canNavigate(frame))
        return nullptr;

    return frame;
}

void FrameLoader::loadSameDocumentItem(HistoryItem& item)
{
    ASSERT(item.documentSequenceNumber() == history().currentItem()->documentSequenceNumber());

    Ref<Frame> protect(m_frame);

    // Save user view state to the current history item here since we don't do a normal load.
    // FIXME: Does form state need to be saved here too?
    history().saveScrollPositionAndViewStateToItem(history().currentItem());
    if (FrameView* view = m_frame.view())
        view->setWasScrolledByUser(false);

    history().setCurrentItem(item);
        
    // loadInSameDocument() actually changes the URL and notifies load delegates of a "fake" load
    loadInSameDocument(item.url(), item.stateObject(), false);

    // Restore user view state from the current history item here since we don't do a normal load.
    history().restoreScrollPositionAndViewState();
}

// FIXME: This function should really be split into a couple pieces, some of
// which should be methods of HistoryController and some of which should be
// methods of FrameLoader.
void FrameLoader::loadDifferentDocumentItem(HistoryItem& item, HistoryItem* fromItem, FrameLoadType loadType, FormSubmissionCacheLoadPolicy cacheLoadPolicy, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    FRAMELOADER_RELEASE_LOG_IF_ALLOWED(ResourceLoading, "loadDifferentDocumentItem: frame load started");

    Ref<Frame> protectedFrame(m_frame);

    // History items should not be reported to the parent.
    m_shouldReportResourceTimingToParentFrame = false;

    // Remember this item so we can traverse any child items as child frames load
    history().setProvisionalItem(&item);

    auto initiatedByMainFrame = InitiatedByMainFrame::Unknown;

    SetForScope<LoadContinuingState> continuingLoadGuard(m_currentLoadContinuingState, shouldTreatAsContinuingLoad == ShouldTreatAsContinuingLoad::Yes ? LoadContinuingState::ContinuingWithHistoryItem : LoadContinuingState::NotContinuing);

    if (CachedPage* cachedPage = BackForwardCache::singleton().get(item, m_frame.page())) {
        auto documentLoader = cachedPage->documentLoader();
        m_client->updateCachedDocumentLoader(*documentLoader);

        auto action = NavigationAction { *m_frame.document(), documentLoader->request(), initiatedByMainFrame, loadType, false };
        action.setTargetBackForwardItem(item);
        action.setSourceBackForwardItem(fromItem);
        documentLoader->setTriggeringAction(WTFMove(action));

        documentLoader->setLastCheckedRequest(ResourceRequest());
        loadWithDocumentLoader(documentLoader, loadType, { }, AllowNavigationToInvalidURL::Yes);
        return;
    }

    URL itemURL = item.url();
    URL itemOriginalURL = item.originalURL();
    URL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->url();
    RefPtr<FormData> formData = item.formData();

    ResourceRequest request(itemURL);

    if (!item.referrer().isNull())
        request.setHTTPReferrer(item.referrer());

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicyToApply(m_frame, initiatedByMainFrame, item.shouldOpenExternalURLsPolicy());
    bool isFormSubmission = false;
    Event* event = nullptr;

    // If this was a repost that failed the page cache, we might try to repost the form.
    NavigationAction action;
    if (formData) {
        request.setHTTPMethod("POST");
        request.setHTTPBody(WTFMove(formData));
        request.setHTTPContentType(item.formContentType());
        auto securityOrigin = SecurityOrigin::createFromString(item.referrer());
        addHTTPOriginIfNeeded(request, securityOrigin->toString());

        addExtraFieldsToRequest(request, IsMainResource::Yes, loadType);
        
        // FIXME: Slight hack to test if the NSURL cache contains the page we're going to.
        // We want to know this before talking to the policy delegate, since it affects whether 
        // we show the DoYouReallyWantToRepost nag.
        //
        // This trick has a small bug (3123893) where we might find a cache hit, but then
        // have the item vanish when we try to use it in the ensuing nav.  This should be
        // extremely rare, but in that case the user will get an error on the navigation.
        
        if (cacheLoadPolicy == MayAttemptCacheOnlyLoadForFormSubmissionItem) {
            request.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataDontLoad);
            action = { *m_frame.document(), request, initiatedByMainFrame, loadType, isFormSubmission, event, shouldOpenExternalURLsPolicy };
        } else {
            request.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataElseLoad);
            action = { *m_frame.document(), request, initiatedByMainFrame, NavigationType::FormResubmitted, shouldOpenExternalURLsPolicy, event };
        }
    } else {
        switch (loadType) {
        case FrameLoadType::Reload:
        case FrameLoadType::ReloadFromOrigin:
        case FrameLoadType::ReloadExpiredOnly:
            request.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);
            break;
        case FrameLoadType::Back:
        case FrameLoadType::Forward:
        case FrameLoadType::IndexedBackForward: {
#if PLATFORM(COCOA)
            bool allowStaleData = true;
#else
            bool allowStaleData = !item.wasRestoredFromSession();
#endif
            if (allowStaleData)
                request.setCachePolicy(ResourceRequestCachePolicy::ReturnCacheDataElseLoad);
            item.setWasRestoredFromSession(false);
            break;
        }
        case FrameLoadType::Standard:
        case FrameLoadType::RedirectWithLockedBackForwardList:
            break;
        case FrameLoadType::Same:
        case FrameLoadType::Replace:
            ASSERT_NOT_REACHED();
        }

        addExtraFieldsToRequest(request, IsMainResource::Yes, loadType);

        ResourceRequest requestForOriginalURL(request);
        requestForOriginalURL.setURL(itemOriginalURL);
        action = { *m_frame.document(), requestForOriginalURL, initiatedByMainFrame, loadType, isFormSubmission, event, shouldOpenExternalURLsPolicy };
    }

    action.setTargetBackForwardItem(item);
    action.setSourceBackForwardItem(fromItem);

    loadWithNavigationAction(request, WTFMove(action), loadType, { }, AllowNavigationToInvalidURL::Yes);
}

// Loads content into this frame, as specified by history item
void FrameLoader::loadItem(HistoryItem& item, HistoryItem* fromItem, FrameLoadType loadType, ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad)
{
    m_requestedHistoryItem = &item;
    HistoryItem* currentItem = history().currentItem();
    bool sameDocumentNavigation = currentItem && item.shouldDoSameDocumentNavigationTo(*currentItem);

    if (sameDocumentNavigation)
        loadSameDocumentItem(item);
    else
        loadDifferentDocumentItem(item, fromItem, loadType, MayAttemptCacheOnlyLoadForFormSubmissionItem, shouldTreatAsContinuingLoad);
}

void FrameLoader::retryAfterFailedCacheOnlyMainResourceLoad()
{
    ASSERT(m_state == FrameStateProvisional);
    ASSERT(!m_loadingFromCachedPage);
    ASSERT(history().provisionalItem());
    ASSERT(history().provisionalItem()->formData());
    ASSERT(history().provisionalItem() == m_requestedHistoryItem.get());

    FrameLoadType loadType = m_loadType;
    HistoryItem* item = history().provisionalItem();

    stopAllLoaders(ShouldNotClearProvisionalItem);
    if (item)
        loadDifferentDocumentItem(*item, history().currentItem(), loadType, MayNotAttemptCacheOnlyLoadForFormSubmissionItem, ShouldTreatAsContinuingLoad::No);
    else {
        ASSERT_NOT_REACHED();
        FRAMELOADER_RELEASE_LOG_ERROR_IF_ALLOWED(ResourceLoading, "retryAfterFailedCacheOnlyMainResourceLoad: Retrying load after failed cache-only main resource load failed because there is no provisional history item.");
    }
}

ResourceError FrameLoader::cancelledError(const ResourceRequest& request) const
{
    ResourceError error = m_client->cancelledError(request);
    error.setType(ResourceError::Type::Cancellation);
    return error;
}

ResourceError FrameLoader::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return m_client->blockedByContentBlockerError(request);
}

ResourceError FrameLoader::blockedError(const ResourceRequest& request) const
{
    ResourceError error = m_client->blockedError(request);
    error.setType(ResourceError::Type::Cancellation);
    return error;
}

#if ENABLE(CONTENT_FILTERING)
ResourceError FrameLoader::blockedByContentFilterError(const ResourceRequest& request) const
{
    ResourceError error = m_client->blockedByContentFilterError(request);
    error.setType(ResourceError::Type::General);
    return error;
}
#endif

#if PLATFORM(IOS_FAMILY)
RetainPtr<CFDictionaryRef> FrameLoader::connectionProperties(ResourceLoader* loader)
{
    return m_client->connectionProperties(loader->documentLoader(), loader->identifier());
}
#endif

ReferrerPolicy FrameLoader::effectiveReferrerPolicy() const
{
    if (auto* parentFrame = m_frame.tree().parent())
        return parentFrame->document()->referrerPolicy();
    if (m_opener)
        return m_opener->document()->referrerPolicy();
    return ReferrerPolicy::NoReferrerWhenDowngrade;
}

String FrameLoader::referrer() const
{
    return m_documentLoader ? m_documentLoader->request().httpReferrer() : emptyString();
}

void FrameLoader::dispatchDidClearWindowObjectsInAllWorlds()
{
    if (!m_frame.script().canExecuteScripts(NotAboutToExecuteScript))
        return;

    Vector<Ref<DOMWrapperWorld>> worlds;
    ScriptController::getAllWorlds(worlds);
    for (auto& world : worlds)
        dispatchDidClearWindowObjectInWorld(world);
}

void FrameLoader::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    if (!m_frame.script().canExecuteScripts(NotAboutToExecuteScript) || !m_frame.windowProxy().existingJSWindowProxy(world))
        return;

    m_client->dispatchDidClearWindowObjectInWorld(world);

    if (Page* page = m_frame.page())
        page->inspectorController().didClearWindowObjectInWorld(m_frame, world);

    InspectorInstrumentation::didClearWindowObjectInWorld(m_frame, world);
}

void FrameLoader::dispatchGlobalObjectAvailableInAllWorlds()
{
    Vector<Ref<DOMWrapperWorld>> worlds;
    ScriptController::getAllWorlds(worlds);
    for (auto& world : worlds)
        m_client->dispatchGlobalObjectAvailable(world);
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
    m_client->didChangeTitle(loader);

    if (loader == m_documentLoader) {
        // Must update the entries in the back-forward list too.
        history().setCurrentItemTitle(loader->title());
        // This must go through the WebFrame because it has the right notion of the current b/f item.
        m_client->setTitle(loader->title(), loader->urlForHistory());
        m_client->setMainFrameDocumentReady(true); // update observers with new DOMDocument
        m_client->dispatchDidReceiveTitle(loader->title());
    }

#if ENABLE(REMOTE_INSPECTOR)
    if (m_frame.isMainFrame())
        m_frame.page()->remoteInspectorInformationDidChange();
#endif
}

void FrameLoader::dispatchDidCommitLoad(Optional<HasInsecureContent> initialHasInsecureContent, Optional<UsedLegacyTLS> initialUsedLegacyTLS)
{
    if (m_stateMachine.creatingInitialEmptyDocument())
        return;

    m_client->dispatchDidCommitLoad(initialHasInsecureContent, initialUsedLegacyTLS);

    if (m_frame.isMainFrame()) {
        m_frame.page()->resetSeenPlugins();
        m_frame.page()->resetSeenMediaEngines();
    }

    InspectorInstrumentation::didCommitLoad(m_frame, m_documentLoader.get());

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

    for (auto& pastLoad : pastLoads) {
        CachedResource* resource = MemoryCache::singleton().resourceForRequest(pastLoad, m_frame.page()->sessionID());

        // FIXME: These loads, loaded from cache, but now gone from the cache by the time
        // Page::setMemoryCacheClientCallsEnabled(true) is called, will not be seen by the client.
        // Consider if there's some efficient way of remembering enough to deliver this client call.
        // We have the URL, but not the rest of the response or the length.
        if (!resource)
            continue;

        ResourceRequest request(resource->url());
        m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize());
    }
}

NetworkingContext* FrameLoader::networkingContext() const
{
    return m_networkingContext.get();
}

void FrameLoader::loadProgressingStatusChanged()
{
    if (auto* view = m_frame.mainFrame().view())
        view->loadProgressingStatusChanged();
}

void FrameLoader::completePageTransitionIfNeeded()
{
    m_client->completePageTransitionIfNeeded();
}

void FrameLoader::clearTestingOverrides()
{
    m_overrideCachePolicyForTesting = WTF::nullopt;
    m_overrideResourceLoadPriorityForTesting = WTF::nullopt;
    m_isStrictRawResourceValidationPolicyDisabledForTesting = false;
}

bool FrameLoader::isAlwaysOnLoggingAllowed() const
{
    return frame().isAlwaysOnLoggingAllowed();
}

bool FrameLoaderClient::hasHTMLView() const
{
    return true;
}

RefPtr<Frame> createWindow(Frame& openerFrame, Frame& lookupFrame, FrameLoadRequest&& request, const WindowFeatures& features, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());
    ASSERT(request.resourceRequest().httpMethod() == "GET");

    created = false;

    // FIXME: Provide line number information with respect to the opener's document.
    if (request.resourceRequest().url().protocolIsJavaScript() && !openerFrame.document()->contentSecurityPolicy()->allowJavaScriptURLs(openerFrame.document()->url().string(), { }))
        return nullptr;

    if (!request.frameName().isEmpty() && !equalIgnoringASCIICase(request.frameName(), "_blank")) {
        if (RefPtr<Frame> frame = lookupFrame.loader().findFrameForNavigation(request.frameName(), openerFrame.document())) {
            if (!equalIgnoringASCIICase(request.frameName(), "_self")) {
                if (Page* page = frame->page())
                    page->chrome().focus();
            }
            return frame;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (isDocumentSandboxed(openerFrame, SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame.document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked opening '" + request.resourceRequest().url().stringCenterEllipsizedToLength() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set.");
        return nullptr;
    }

    // FIXME: Setting the referrer should be the caller's responsibility.
    String referrer = SecurityPolicy::generateReferrerHeader(openerFrame.document()->referrerPolicy(), request.resourceRequest().url(), openerFrame.loader().outgoingReferrer());
    if (!referrer.isEmpty())
        request.resourceRequest().setHTTPReferrer(referrer);
    FrameLoader::addSameSiteInfoToRequestIfNeeded(request.resourceRequest(), openerFrame.document());

    Page* oldPage = openerFrame.page();
    if (!oldPage)
        return nullptr;

    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicyToApply(openerFrame, request);
    NavigationAction action { request.requester(), request.resourceRequest(), request.initiatedByMainFrame(), NavigationType::Other, shouldOpenExternalURLsPolicy };
    Page* page = oldPage->chrome().createWindow(openerFrame, features, action);
    if (!page)
        return nullptr;

    RefPtr<Frame> frame = &page->mainFrame();

    if (isDocumentSandboxed(openerFrame, SandboxPropagatesToAuxiliaryBrowsingContexts))
        frame->loader().forceSandboxFlags(openerFrame.document()->sandboxFlags());

    if (!equalIgnoringASCIICase(request.frameName(), "_blank"))
        frame->tree().setName(request.frameName());

    page->chrome().setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);

    if (!frame->page())
        return nullptr;
    page->chrome().setStatusbarVisible(features.statusBarVisible);

    if (!frame->page())
        return nullptr;
    page->chrome().setScrollbarsVisible(features.scrollbarsVisible);

    if (!frame->page())
        return nullptr;
    page->chrome().setMenubarVisible(features.menuBarVisible);

    if (!frame->page())
        return nullptr;
    page->chrome().setResizable(features.resizable);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

    // FIXME: We should reconcile the initialization of viewport arguments between iOS and non-IOS.
#if !PLATFORM(IOS_FAMILY)
    FloatSize viewportSize = page->chrome().pageRect().size();
    FloatRect windowRect = page->chrome().windowRect();
    if (features.x)
        windowRect.setX(*features.x);
    if (features.y)
        windowRect.setY(*features.y);
    // Zero width and height mean using default size, not minimum one.
    if (features.width && *features.width)
        windowRect.setWidth(*features.width + (windowRect.width() - viewportSize.width()));
    if (features.height && *features.height)
        windowRect.setHeight(*features.height + (windowRect.height() - viewportSize.height()));

#if PLATFORM(GTK)
    FloatRect oldWindowRect = oldPage->chrome().windowRect();
    // Use the size of the previous window if there is no default size.
    if (!windowRect.width())
        windowRect.setWidth(oldWindowRect.width());
    if (!windowRect.height())
        windowRect.setHeight(oldWindowRect.height());
#endif

    // Ensure non-NaN values, minimum size as well as being within valid screen area.
    FloatRect newWindowRect = DOMWindow::adjustWindowRect(*page, windowRect);

    if (!frame->page())
        return nullptr;
    page->chrome().setWindowRect(newWindowRect);
#else
    // On iOS, width and height refer to the viewport dimensions.
    ViewportArguments arguments;
    // Zero width and height mean using default size, not minimum one.
    if (features.width && *features.width)
        arguments.width = *features.width;
    if (features.height && *features.height)
        arguments.height = *features.height;
    frame->setViewportArguments(arguments);
#endif

    if (!frame->page())
        return nullptr;
    page->chrome().show();

    created = true;
    return frame;
}

bool FrameLoader::shouldSuppressTextInputFromEditing() const
{
    return m_frame.settings().shouldSuppressTextInputFromEditingDuringProvisionalNavigation() && m_state == FrameStateProvisional;
}

bool FrameLoader::arePluginsEnabled()
{
    return m_frame.settings().arePluginsEnabled();
}

} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
