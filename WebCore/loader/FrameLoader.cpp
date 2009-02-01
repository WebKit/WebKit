/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#include "Archive.h"
#include "ArchiveFactory.h"
#include "CString.h"
#include "Cache.h"
#include "CachedPage.h"
#include "Chrome.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTTPParsers.h"
#include "HistoryItem.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "InspectorController.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "PluginData.h"
#include "PluginDocument.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "TextResourceDecoder.h"
#include "WindowFeatures.h"
#include "XMLHttpRequest.h"
#include "XMLTokenizer.h"
#include <wtf/CurrentTime.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "ApplicationCache.h"
#include "ApplicationCacheResource.h"
#endif

#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#endif

namespace WebCore {

#if ENABLE(SVG)
using namespace SVGNames;
#endif
using namespace HTMLNames;

#if USE(LOW_BANDWIDTH_DISPLAY)
const unsigned int cMaxPendingSourceLengthInLowBandwidthDisplay = 128 * 1024;
#endif

typedef HashSet<String, CaseFoldingHash> LocalSchemesMap;

struct FormSubmission {
    FormSubmission(const char* action, const String& url, PassRefPtr<FormData> formData,
                   const String& target, const String& contentType, const String& boundary,
                   PassRefPtr<Event> event, bool lockHistory, bool lockBackForwardList)
        : action(action)
        , url(url)
        , formData(formData)
        , target(target)
        , contentType(contentType)
        , boundary(boundary)
        , event(event)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
    {
    }

    const char* action;
    String url;
    RefPtr<FormData> formData;
    String target;
    String contentType;
    String boundary;
    RefPtr<Event> event;
    bool lockHistory;
    bool lockBackForwardList;
};

struct ScheduledRedirection {
    enum Type { redirection, locationChange, historyNavigation, locationChangeDuringLoad };
    Type type;
    double delay;
    String url;
    String referrer;
    int historySteps;
    bool lockHistory;
    bool lockBackForwardList;
    bool wasUserGesture;
    bool wasRefresh;

    ScheduledRedirection(double delay, const String& url, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool refresh)
        : type(redirection)
        , delay(delay)
        , url(url)
        , historySteps(0)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
        , wasUserGesture(wasUserGesture)
        , wasRefresh(refresh)
    {
    }

    ScheduledRedirection(Type locationChangeType, const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture, bool refresh)
        : type(locationChangeType)
        , delay(0)
        , url(url)
        , referrer(referrer)
        , historySteps(0)
        , lockHistory(lockHistory)
        , lockBackForwardList(lockBackForwardList)
        , wasUserGesture(wasUserGesture)
        , wasRefresh(refresh)
    {
    }

    explicit ScheduledRedirection(int historyNavigationSteps)
        : type(historyNavigation)
        , delay(0)
        , historySteps(historyNavigationSteps)
        , lockHistory(false)
        , wasUserGesture(false)
        , wasRefresh(false)
    {
    }
};

static double storedTimeOfLastCompletedLoad;
static FrameLoader::LocalLoadPolicy localLoadPolicy = FrameLoader::AllowLocalLoadsForLocalOnly;

bool isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
        case FrameLoadTypeStandard:
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeRedirect:
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

static int numRequests(Document* document)
{
    if (!document)
        return 0;
    
    return document->docLoader()->requestCount();
}

FrameLoader::FrameLoader(Frame* frame, FrameLoaderClient* client)
    : m_frame(frame)
    , m_client(client)
    , m_state(FrameStateCommittedPage)
    , m_loadType(FrameLoadTypeStandard)
    , m_policyLoadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_delegateIsDecidingNavigationPolicy(false)
    , m_delegateIsHandlingUnimplementablePolicy(false)
    , m_firstLayoutDone(false)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_inStopAllLoaders(false)
    , m_navigationDuringLoad(false)
    , m_isExecutingJavaScriptFormAction(false)
    , m_isRunningScript(false)
    , m_didCallImplicitClose(false)
    , m_wasUnloadEventEmitted(false)
    , m_isComplete(false)
    , m_isLoadingMainResource(false)
    , m_cancellingWithLoadInProgress(false)
    , m_needsClear(false)
    , m_receivedData(false)
    , m_encodingWasChosenByUser(false)
    , m_containsPlugIns(false)
    , m_redirectionTimer(this, &FrameLoader::redirectionTimerFired)
    , m_checkCompletedTimer(this, &FrameLoader::checkCompletedTimerFired)
    , m_checkLoadCompleteTimer(this, &FrameLoader::checkLoadCompleteTimerFired)
    , m_opener(0)
    , m_openedByDOM(false)
    , m_creatingInitialEmptyDocument(false)
    , m_isDisplayingInitialEmptyDocument(false)
    , m_committedFirstRealDocumentLoad(false)
    , m_didPerformFirstNavigation(false)
#ifndef NDEBUG
    , m_didDispatchDidCommitLoad(false)
#endif
#if USE(LOW_BANDWIDTH_DISPLAY)
    , m_useLowBandwidthDisplay(true)
    , m_finishedParsingDuringLowBandwidthDisplay(false)
    , m_needToSwitchOutLowBandwidthDisplay(false)
#endif 
#if ENABLE(WML)
    , m_forceReloadWmlDeck(false)
#endif
{
}

FrameLoader::~FrameLoader()
{
    setOpener(0);

    HashSet<Frame*>::iterator end = m_openedFrames.end();
    for (HashSet<Frame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->loader()->m_opener = 0;
        
    m_client->frameLoaderDestroyed();
}

void FrameLoader::init()
{
    // this somewhat odd set of steps is needed to give the frame an initial empty document
    m_isDisplayingInitialEmptyDocument = false;
    m_creatingInitialEmptyDocument = true;
    setPolicyDocumentLoader(m_client->createDocumentLoader(ResourceRequest(KURL("")), SubstituteData()).get());
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    setState(FrameStateProvisional);
    m_provisionalDocumentLoader->setResponse(ResourceResponse(KURL(), "text/html", 0, String(), String()));
    m_provisionalDocumentLoader->finishedLoading();
    begin(KURL(), false);
    end();
    m_frame->document()->cancelParsing();
    m_creatingInitialEmptyDocument = false;
    m_didCallImplicitClose = true;
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_documentLoader)
        m_documentLoader->setDefersLoading(defers);
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->setDefersLoading(defers);
    if (m_policyDocumentLoader)
        m_policyDocumentLoader->setDefersLoading(defers);
}

Frame* FrameLoader::createWindow(FrameLoader* frameLoaderForFrameLookup, const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{ 
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank") {
        Frame* frame = frameLoaderForFrameLookup->frame()->tree()->find(request.frameName());
        if (frame && shouldAllowNavigation(frame)) {
            if (!request.resourceRequest().url().isEmpty())
                frame->loader()->loadFrameRequestWithFormAndValues(request, false, false, 0, 0, HashMap<String, String>());
            if (Page* page = frame->page())
                page->chrome()->focus();
            created = false;
            return frame;
        }
    }
    
    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    requestWithReferrer.resourceRequest().setHTTPReferrer(m_outgoingReferrer);
    addHTTPOriginIfNeeded(requestWithReferrer.resourceRequest(), outgoingOrigin());

    Page* oldPage = m_frame->page();
    if (!oldPage)
        return 0;
        
    Page* page = oldPage->chrome()->createWindow(m_frame, requestWithReferrer, features);
    if (!page)
        return 0;

    Frame* frame = page->mainFrame();
    if (request.frameName() != "_blank")
        frame->tree()->setName(request.frameName());

    page->chrome()->setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    page->chrome()->setStatusbarVisible(features.statusBarVisible);
    page->chrome()->setScrollbarsVisible(features.scrollbarsVisible);
    page->chrome()->setMenubarVisible(features.menuBarVisible);
    page->chrome()->setResizable(features.resizable);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height' 
    // specify the size of the page. We can only resize the window, so 
    // adjust for the difference between the window size and the page size.

    FloatRect windowRect = page->chrome()->windowRect();
    FloatSize pageSize = page->chrome()->pageRect().size();
    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - pageSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - pageSize.height()));
    page->chrome()->setWindowRect(windowRect);

    page->chrome()->show();

    created = true;
    return frame;
}

bool FrameLoader::canHandleRequest(const ResourceRequest& request)
{
    return m_client->canHandleRequest(request);
}

void FrameLoader::changeLocation(const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool userGesture, bool refresh)
{
    changeLocation(completeURL(url), referrer, lockHistory, lockBackForwardList, userGesture, refresh);
}


void FrameLoader::changeLocation(const KURL& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool userGesture, bool refresh)
{
    RefPtr<Frame> protect(m_frame);

    ResourceRequest request(url, referrer, refresh ? ReloadIgnoringCacheData : UseProtocolCachePolicy);
    
    if (executeIfJavaScriptURL(request.url(), userGesture))
        return;

    urlSelected(request, "_self", 0, lockHistory, lockBackForwardList, userGesture);
}

void FrameLoader::urlSelected(const FrameLoadRequest& request, Event* event, bool lockHistory, bool lockBackForwardList)
{
    FrameLoadRequest copy = request;
    if (copy.resourceRequest().httpReferrer().isEmpty())
        copy.resourceRequest().setHTTPReferrer(m_outgoingReferrer);
    addHTTPOriginIfNeeded(copy.resourceRequest(), outgoingOrigin());

    loadFrameRequestWithFormAndValues(copy, lockHistory, lockBackForwardList, event, 0, HashMap<String, String>());
}
    
void FrameLoader::urlSelected(const ResourceRequest& request, const String& _target, Event* triggeringEvent, bool lockHistory, bool lockBackForwardList, bool userGesture)
{
    if (executeIfJavaScriptURL(request.url(), userGesture, false))
        return;

    String target = _target;
    if (target.isEmpty() && m_frame->document())
        target = m_frame->document()->baseTarget();

    FrameLoadRequest frameRequest(request, target);

    urlSelected(frameRequest, triggeringEvent, lockHistory, lockBackForwardList);
}

bool FrameLoader::requestFrame(HTMLFrameOwnerElement* ownerElement, const String& urlString, const AtomicString& frameName)
{
#if USE(LOW_BANDWIDTH_DISPLAY)
    // don't create sub-frame during low bandwidth display
    if (frame()->document()->inLowBandwidthDisplay()) {
        m_needToSwitchOutLowBandwidthDisplay = true;
        return false;
    }
#endif

    // Support for <frame src="javascript:string">
    KURL scriptURL;
    KURL url;
    if (protocolIs(urlString, "javascript")) {
        scriptURL = completeURL(urlString); // completeURL() encodes the URL.
        url = blankURL();
    } else
        url = completeURL(urlString);

    Frame* frame = ownerElement->contentFrame();
    if (frame)
        frame->loader()->scheduleLocationChange(url.string(), m_outgoingReferrer, true, userGestureHint());
    else
        frame = loadSubframe(ownerElement, url, frameName, m_outgoingReferrer);
    
    if (!frame)
        return false;

    if (!scriptURL.isEmpty())
        frame->loader()->executeIfJavaScriptURL(scriptURL);

    return true;
}

Frame* FrameLoader::loadSubframe(HTMLFrameOwnerElement* ownerElement, const KURL& url, const String& name, const String& referrer)
{
    bool allowsScrolling = true;
    int marginWidth = -1;
    int marginHeight = -1;
    if (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag)) {
        HTMLFrameElementBase* o = static_cast<HTMLFrameElementBase*>(ownerElement);
        allowsScrolling = o->scrollingMode() != ScrollbarAlwaysOff;
        marginWidth = o->getMarginWidth();
        marginHeight = o->getMarginHeight();
    }

    if (!canLoad(url, referrer)) {
        FrameLoader::reportLocalLoadFailed(m_frame, url.string());
        return 0;
    }

    bool hideReferrer = shouldHideReferrer(url, referrer);
    RefPtr<Frame> frame = m_client->createFrame(url, name, ownerElement, hideReferrer ? String() : referrer,
                                         allowsScrolling, marginWidth, marginHeight);

    if (!frame)  {
        checkCallImplicitClose();
        return 0;
    }
    
    frame->loader()->m_isComplete = false;
   
    RenderObject* renderer = ownerElement->renderer();
    FrameView* view = frame->view();
    if (renderer && renderer->isWidget() && view)
        static_cast<RenderWidget*>(renderer)->setWidget(view);
    
    checkCallImplicitClose();
    
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the 
    // completed() signal for the child by hand
    // FIXME: In this case the Frame will have finished loading before 
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (url.isEmpty() || url == blankURL()) {
        frame->loader()->completed();
        frame->loader()->checkCompleted();
    }

    return frame.get();
}

void FrameLoader::submitFormAgain()
{
    if (m_isRunningScript)
        return;
    OwnPtr<FormSubmission> form(m_deferredFormSubmission.release());
    if (!form)
        return;
    submitForm(form->action, form->url, form->formData, form->target, form->contentType, form->boundary, form->event.get(), form->lockHistory, form->lockBackForwardList);
}

void FrameLoader::submitForm(const char* action, const String& url, PassRefPtr<FormData> formData,
    const String& target, const String& contentType, const String& boundary, Event* event, bool lockHistory, bool lockBackForwardList)
{
    ASSERT(formData);
    
    if (!m_frame->page())
        return;
    
    KURL u = completeURL(url.isNull() ? "" : url);
    // FIXME: Do we really need to special-case an empty URL?
    // Would it be better to just go on with the form submisson and let the I/O fail?
    if (u.isEmpty())
        return;

    if (u.protocolIs("javascript")) {
        m_isExecutingJavaScriptFormAction = true;
        executeIfJavaScriptURL(u, false, false);
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    if (m_isRunningScript) {
        if (m_deferredFormSubmission)
            return;
        m_deferredFormSubmission.set(new FormSubmission(action, url, formData, target, contentType, boundary, event, lockHistory, lockBackForwardList));
        return;
    }

    formData->generateFiles(m_frame->page()->chrome()->client());
    
    FrameLoadRequest frameRequest;

    if (!m_outgoingReferrer.isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    frameRequest.setFrameName(target.isEmpty() ? m_frame->document()->baseTarget() : target);

    // Handle mailto: forms
    bool isMailtoForm = equalIgnoringCase(u.protocol(), "mailto");
    if (isMailtoForm && strcmp(action, "GET") != 0) {
        // Append body= for POST mailto, replace the whole query string for GET one.
        String body = formData->flattenToString();
        String query = u.query();
        if (!query.isEmpty())
            query.append('&');
        u.setQuery(query + body);
    }

    if (strcmp(action, "GET") == 0) {
        u.setQuery(formData->flattenToString());
    } else {
        if (!isMailtoForm)
            frameRequest.resourceRequest().setHTTPBody(formData.get());
        frameRequest.resourceRequest().setHTTPMethod("POST");

        // construct some user headers if necessary
        if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
            frameRequest.resourceRequest().setHTTPContentType(contentType);
        else // contentType must be "multipart/form-data"
            frameRequest.resourceRequest().setHTTPContentType(contentType + "; boundary=" + boundary);
    }

    frameRequest.resourceRequest().setURL(u);
    addHTTPOriginIfNeeded(frameRequest.resourceRequest(), outgoingOrigin());

    submitForm(frameRequest, event, lockHistory, lockBackForwardList);
}

void FrameLoader::stopLoading(bool sendUnload)
{
    if (m_frame->document() && m_frame->document()->tokenizer())
        m_frame->document()->tokenizer()->stopParsing();

    if (sendUnload) {
        if (m_frame->document()) {
            if (m_didCallImplicitClose && !m_wasUnloadEventEmitted) {
                Node* currentFocusedNode = m_frame->document()->focusedNode();
                if (currentFocusedNode)
                    currentFocusedNode->aboutToUnload();
                m_frame->document()->dispatchWindowEvent(eventNames().unloadEvent, false, false);
                if (m_frame->document())
                    m_frame->document()->updateRendering();
                m_wasUnloadEventEmitted = true;
                if (m_frame->eventHandler()->pendingFrameUnloadEventCount())
                    m_frame->eventHandler()->clearPendingFrameUnloadEventCount();
                if (m_frame->eventHandler()->pendingFrameBeforeUnloadEventCount())
                    m_frame->eventHandler()->clearPendingFrameBeforeUnloadEventCount();
            }
        }
        if (m_frame->document() && !m_frame->document()->inPageCache())
            m_frame->document()->removeAllEventListenersFromAllNodes();
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing() (David)
    m_isLoadingMainResource = false;
    m_didCallImplicitClose = true; // don't want that one either

    if (m_frame->document() && m_frame->document()->parsing()) {
        finishedParsing();
        m_frame->document()->setParsing(false);
    }
  
    m_workingURL = KURL();

    if (Document* doc = m_frame->document()) {
        if (DocLoader* docLoader = doc->docLoader())
            cache()->loader()->cancelRequests(docLoader);

#if ENABLE(DATABASE)
        doc->stopDatabases();
#endif
    }

    // tell all subframes to stop as well
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopLoading(sendUnload);

    cancelRedirection();

#if USE(LOW_BANDWIDTH_DISPLAY)
    if (m_frame->document() && m_frame->document()->inLowBandwidthDisplay()) {
        // Since loading is forced to stop, reset the state without really switching.
        m_needToSwitchOutLowBandwidthDisplay = false;
        switchOutLowBandwidthDisplayIfReady();
    }
#endif  
}

void FrameLoader::stop()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it will be deleted by checkCompleted().
    RefPtr<Frame> protector(m_frame);
    
    if (m_frame->document()) {
        if (m_frame->document()->tokenizer())
            m_frame->document()->tokenizer()->stopParsing();
        m_frame->document()->finishParsing();
    } else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true. An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
    if (m_iconLoader)
        m_iconLoader->stopLoading();
}

bool FrameLoader::closeURL()
{
    saveDocumentState();
    stopLoading(true);
    m_frame->editor()->clearUndoRedoOperations();
    return true;
}

void FrameLoader::cancelRedirection(bool cancelWithLoadInProgress)
{
    m_cancellingWithLoadInProgress = cancelWithLoadInProgress;

    stopRedirectionTimer();

    m_scheduledRedirection.clear();
}

KURL FrameLoader::iconURL()
{
    // If this isn't a top level frame, return nothing
    if (m_frame->tree() && m_frame->tree()->parent())
        return KURL();

    // If we have an iconURL from a Link element, return that
    if (m_frame->document() && !m_frame->document()->iconURL().isEmpty())
        return KURL(m_frame->document()->iconURL());

    // Don't return a favicon iconURL unless we're http or https
    if (!m_URL.protocolIs("http") && !m_URL.protocolIs("https"))
        return KURL();

    KURL url;
    url.setProtocol(m_URL.protocol());
    url.setHost(m_URL.host());
    if (int port = m_URL.port())
        url.setPort(port);
    url.setPath("/favicon.ico");
    return url;
}

bool FrameLoader::didOpenURL(const KURL& url)
{
    if (m_scheduledRedirection && m_scheduledRedirection->type == ScheduledRedirection::locationChangeDuringLoad)
        // A redirect was scheduled before the document was created.
        // This can happen when one frame changes another frame's location.
        return false;

    cancelRedirection();
    m_frame->editor()->clearLastEditCommand();
    closeURL();

    m_isComplete = false;
    m_isLoadingMainResource = true;
    m_didCallImplicitClose = false;

    m_frame->setJSStatusBarText(String());
    m_frame->setJSDefaultStatusBarText(String());

    m_URL = url;
    if ((m_URL.protocolIs("http") || m_URL.protocolIs("https")) && !m_URL.host().isEmpty() && m_URL.path().isEmpty())
        m_URL.setPath("/");
    m_workingURL = m_URL;

    started();

    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;
    m_didCallImplicitClose = false;

    // Calling document.open counts as committing the first real document load.
    m_committedFirstRealDocumentLoad = true;
    
    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call. 
    // Cancelling redirection here works for all cases because document.open 
    // implicitly precedes document.write.
    cancelRedirection(); 
    if (m_frame->document()->url() != blankURL())
        m_URL = m_frame->document()->url();
}

bool FrameLoader::executeIfJavaScriptURL(const KURL& url, bool userGesture, bool replaceDocument)
{
    if (!url.protocolIs("javascript"))
        return false;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    ScriptValue result = executeScript(script, userGesture);

    String scriptResult;
    if (!result.getString(scriptResult))
        return true;

    SecurityOrigin* currentSecurityOrigin = 0;
    if (m_frame->document())
        currentSecurityOrigin = m_frame->document()->securityOrigin();

    // FIXME: We should always replace the document, but doing so
    //        synchronously can cause crashes:
    //        http://bugs.webkit.org/show_bug.cgi?id=16782
    if (replaceDocument) {
        begin(m_URL, true, currentSecurityOrigin);
        write(scriptResult);
        end();
    }

    return true;
}

ScriptValue FrameLoader::executeScript(const String& script, bool forceUserGesture)
{
    return executeScript(ScriptSourceCode(script, forceUserGesture ? KURL() : m_URL));
}

ScriptValue FrameLoader::executeScript(const ScriptSourceCode& sourceCode)
{
    if (!m_frame->script()->isEnabled() || m_frame->script()->isPaused())
        return ScriptValue();

    bool wasRunningScript = m_isRunningScript;
    m_isRunningScript = true;

    ScriptValue result = m_frame->script()->evaluate(sourceCode);

    if (!wasRunningScript) {
        m_isRunningScript = false;
        submitFormAgain();
        Document::updateDocumentsRendering();
    }

    return result;
}

void FrameLoader::cancelAndClear()
{
    cancelRedirection();

    if (!m_isComplete)
        closeURL();

    clear(false);
    m_frame->script()->updatePlatformScriptObjects();
}

void FrameLoader::clear(bool clearWindowProperties, bool clearScriptObjects)
{
    m_frame->editor()->clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;
    
    if (m_frame->document() && !m_frame->document()->inPageCache()) {
        m_frame->document()->cancelParsing();
        m_frame->document()->stopActiveDOMObjects();
        if (m_frame->document()->attached()) {
            m_frame->document()->willRemove();
            m_frame->document()->detach();
            
            m_frame->document()->removeFocusedNodeOfSubtree(m_frame->document());
        }
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        m_frame->clearDOMWindow();
        m_frame->script()->clearWindowShell();
    }

    m_frame->selection()->clear();
    m_frame->eventHandler()->clear();
    if (m_frame->view())
        m_frame->view()->clear();

    m_frame->setSelectionGranularity(CharacterGranularity);

    // Do not drop the document before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame->setDocument(0);
    m_decoder = 0;

    m_containsPlugIns = false;

    if (clearScriptObjects)
        m_frame->script()->clearScriptObjects();

    m_redirectionTimer.stop();
    m_scheduledRedirection.clear();

    m_checkCompletedTimer.stop();
    m_checkLoadCompleteTimer.stop();

    m_receivedData = false;
    m_isDisplayingInitialEmptyDocument = false;

    if (!m_encodingWasChosenByUser)
        m_encoding = String();
}

void FrameLoader::receivedFirstData()
{
    begin(m_workingURL, false);

    dispatchDidCommitLoad();
    dispatchWindowObjectAvailable();
    
    String ptitle = m_documentLoader->title();
    // If we have a title let the WebView know about it.
    if (!ptitle.isNull())
        m_client->dispatchDidReceiveTitle(ptitle);

    m_workingURL = KURL();

    double delay;
    String url;
    if (!m_documentLoader)
        return;
    if (!parseHTTPRefresh(m_documentLoader->response().httpHeaderField("Refresh"), false, delay, url))
        return;

    if (url.isEmpty())
        url = m_URL.string();
    else
        url = m_frame->document()->completeURL(url).string();

    scheduleHTTPRedirection(delay, url);
}

const String& FrameLoader::responseMIMEType() const
{
    return m_responseMIMEType;
}

void FrameLoader::setResponseMIMEType(const String& type)
{
    m_responseMIMEType = type;
}
    
void FrameLoader::begin()
{
    begin(KURL());
}

void FrameLoader::begin(const KURL& url, bool dispatch, SecurityOrigin* origin)
{
    // We need to take a reference to the security origin because |clear|
    // might destroy the document that owns it.
    RefPtr<SecurityOrigin> forcedSecurityOrigin = origin;

    bool resetScripting = !(m_isDisplayingInitialEmptyDocument && m_frame->document() && m_frame->document()->securityOrigin()->isSecureTransitionTo(url));
    clear(resetScripting, resetScripting);
    if (resetScripting)
        m_frame->script()->updatePlatformScriptObjects();
    if (dispatch)
        dispatchWindowObjectAvailable();

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_isLoadingMainResource = true;
    m_isDisplayingInitialEmptyDocument = m_creatingInitialEmptyDocument;

    KURL ref(url);
    ref.setUser(String());
    ref.setPass(String());
    ref.setRef(String());
    m_outgoingReferrer = ref.string();
    m_URL = url;

    RefPtr<Document> document;
    
    if (!m_isDisplayingInitialEmptyDocument && m_client->shouldUsePluginDocument(m_responseMIMEType))
        document = PluginDocument::create(m_frame);
    else
        document = DOMImplementation::createDocument(m_responseMIMEType, m_frame, m_frame->inViewSourceMode());
    m_frame->setDocument(document);

    document->setURL(m_URL);
    if (m_decoder)
        document->setDecoder(m_decoder.get());
    if (forcedSecurityOrigin)
        document->setSecurityOrigin(forcedSecurityOrigin.get());

    m_frame->domWindow()->setURL(document->url());
    m_frame->domWindow()->setSecurityOrigin(document->securityOrigin());

    updatePolicyBaseURL();

    Settings* settings = document->settings();
    document->docLoader()->setAutoLoadImages(settings && settings->loadsImagesAutomatically());

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            document->parseDNSPrefetchControlHeader(dnsPrefetchControl);
    }

#if FRAME_LOADS_USER_STYLESHEET
    KURL userStyleSheet = settings ? settings->userStyleSheetLocation() : KURL();
    if (!userStyleSheet.isEmpty())
        m_frame->setUserStyleSheetLocation(userStyleSheet);
#endif

    restoreDocumentState();

    document->implicitOpen();

    if (m_frame->view())
        m_frame->view()->setContentsSize(IntSize());

#if USE(LOW_BANDWIDTH_DISPLAY)
    // Low bandwidth display is a first pass display without external resources
    // used to give an instant visual feedback. We currently only enable it for
    // HTML documents in the top frame.
    if (document->isHTMLDocument() && !m_frame->tree()->parent() && m_useLowBandwidthDisplay) {
        m_pendingSourceInLowBandwidthDisplay = String();
        m_finishedParsingDuringLowBandwidthDisplay = false;
        m_needToSwitchOutLowBandwidthDisplay = false;
        document->setLowBandwidthDisplay(true);
    }
#endif
}

void FrameLoader::write(const char* str, int len, bool flush)
{
    if (len == 0 && !flush)
        return;
    
    if (len == -1)
        len = strlen(str);

    Tokenizer* tokenizer = m_frame->document()->tokenizer();
    if (tokenizer && tokenizer->wantsRawData()) {
        if (len > 0)
            tokenizer->writeRawData(str, len);
        return;
    }
    
    if (!m_decoder) {
        Settings* settings = m_frame->settings();
        m_decoder = TextResourceDecoder::create(m_responseMIMEType, settings ? settings->defaultTextEncodingName() : String());
        if (m_encoding.isEmpty()) {
            Frame* parentFrame = m_frame->tree()->parent();
            if (parentFrame && parentFrame->document()->securityOrigin()->canAccess(m_frame->document()->securityOrigin()))
                m_decoder->setEncoding(parentFrame->document()->inputEncoding(), TextResourceDecoder::DefaultEncoding);
        } else {
            m_decoder->setEncoding(m_encoding,
                m_encodingWasChosenByUser ? TextResourceDecoder::UserChosenEncoding : TextResourceDecoder::EncodingFromHTTPHeader);
        }
        m_frame->document()->setDecoder(m_decoder.get());
    }

    String decoded = m_decoder->decode(str, len);
    if (flush)
        decoded += m_decoder->flush();
    if (decoded.isEmpty())
        return;

#if USE(LOW_BANDWIDTH_DISPLAY)
    if (m_frame->document()->inLowBandwidthDisplay())
        m_pendingSourceInLowBandwidthDisplay.append(decoded);   
#endif

    if (!m_receivedData) {
        m_receivedData = true;
        if (m_decoder->encoding().usesVisualOrdering())
            m_frame->document()->setVisuallyOrdered();
        m_frame->document()->recalcStyle(Node::Force);
    }

    if (tokenizer) {
        ASSERT(!tokenizer->wantsRawData());
        tokenizer->write(decoded, true);
    }
}

void FrameLoader::write(const String& str)
{
    if (str.isNull())
        return;

    if (!m_receivedData) {
        m_receivedData = true;
        m_frame->document()->setParseMode(Document::Strict);
    }

    if (Tokenizer* tokenizer = m_frame->document()->tokenizer())
        tokenizer->write(str, true);
}

void FrameLoader::end()
{
    m_isLoadingMainResource = false;
    endIfNotLoadingMainResource();
}

void FrameLoader::endIfNotLoadingMainResource()
{
    if (m_isLoadingMainResource || !m_frame->page())
        return;

    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(), 
    // so we'll add a protective refcount
    RefPtr<Frame> protector(m_frame);

    // make sure nothing's left in there
    if (m_frame->document()) {
        write(0, 0, true);
        m_frame->document()->finishParsing();
#if USE(LOW_BANDWIDTH_DISPLAY)
        if (m_frame->document()->inLowBandwidthDisplay()) {
            m_finishedParsingDuringLowBandwidthDisplay = true;
            switchOutLowBandwidthDisplayIfReady();
        }
#endif            
    } else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();
}

void FrameLoader::iconLoadDecisionAvailable()
{
    if (!m_mayLoadIconLater)
        return;
    LOG(IconDatabase, "FrameLoader %p was told a load decision is available for its icon", this);
    startIconLoader();
    m_mayLoadIconLater = false;
}

void FrameLoader::startIconLoader()
{
    // FIXME: We kick off the icon loader when the frame is done receiving its main resource.
    // But we should instead do it when we're done parsing the head element.
    if (!isLoadingMainFrame())
        return;

    if (!iconDatabase() || !iconDatabase()->isEnabled())
        return;
    
    KURL url(iconURL());
    String urlString(url.string());
    if (urlString.isEmpty())
        return;

    // If we're not reloading and the icon database doesn't say to load now then bail before we actually start the load
    if (loadType() != FrameLoadTypeReload && loadType() != FrameLoadTypeReloadFromOrigin) {
        IconLoadDecision decision = iconDatabase()->loadDecisionForIconURL(urlString, m_documentLoader.get());
        if (decision == IconLoadNo) {
            LOG(IconDatabase, "FrameLoader::startIconLoader() - Told not to load this icon, committing iconURL %s to database for pageURL mapping", urlString.ascii().data());
            commitIconURLToIconDatabase(url);
            
            // We were told not to load this icon - that means this icon is already known by the database
            // If the icon data hasn't been read in from disk yet, kick off the read of the icon from the database to make sure someone
            // has done it.  This is after registering for the notification so the WebView can call the appropriate delegate method.
            // Otherwise if the icon data *is* available, notify the delegate
            if (!iconDatabase()->iconDataKnownForIconURL(urlString)) {
                LOG(IconDatabase, "Told not to load icon %s but icon data is not yet available - registering for notification and requesting load from disk", urlString.ascii().data());
                m_client->registerForIconNotification();
                iconDatabase()->iconForPageURL(m_URL.string(), IntSize(0, 0));
                iconDatabase()->iconForPageURL(originalRequestURL().string(), IntSize(0, 0));
            } else
                m_client->dispatchDidReceiveIcon();
                
            return;
        } 
        
        if (decision == IconLoadUnknown) {
            // In this case, we may end up loading the icon later, but we still want to commit the icon url mapping to the database
            // just in case we don't end up loading later - if we commit the mapping a second time after the load, that's no big deal
            // We also tell the client to register for the notification that the icon is received now so it isn't missed in case the 
            // icon is later read in from disk
            LOG(IconDatabase, "FrameLoader %p might load icon %s later", this, urlString.ascii().data());
            m_mayLoadIconLater = true;    
            m_client->registerForIconNotification();
            commitIconURLToIconDatabase(url);
            return;
        }
    }

    // This is either a reload or the icon database said "yes, load the icon", so kick off the load!
    if (!m_iconLoader)
        m_iconLoader.set(IconLoader::create(m_frame).release());
        
    m_iconLoader->startLoading();
}

void FrameLoader::setLocalLoadPolicy(LocalLoadPolicy policy)
{
    localLoadPolicy = policy;
}

bool FrameLoader::restrictAccessToLocal()
{
    return localLoadPolicy != FrameLoader::AllowLocalLoadsForAll;
}

bool FrameLoader::allowSubstituteDataAccessToLocal()
{
    return localLoadPolicy != FrameLoader::AllowLocalLoadsForLocalOnly;
}

static LocalSchemesMap& localSchemes()
{
    DEFINE_STATIC_LOCAL(LocalSchemesMap, localSchemes, ());

    if (localSchemes.isEmpty()) {
        localSchemes.add("file");
#if PLATFORM(MAC)
        localSchemes.add("applewebdata");
#endif
#if PLATFORM(QT)
        localSchemes.add("qrc");
#endif
    }

    return localSchemes;
}

void FrameLoader::commitIconURLToIconDatabase(const KURL& icon)
{
    ASSERT(iconDatabase());
    LOG(IconDatabase, "Committing iconURL %s to database for pageURLs %s and %s", icon.string().ascii().data(), m_URL.string().ascii().data(), originalRequestURL().string().ascii().data());
    iconDatabase()->setIconURLForPageURL(icon.string(), m_URL.string());
    iconDatabase()->setIconURLForPageURL(icon.string(), originalRequestURL().string());
}

void FrameLoader::restoreDocumentState()
{
    Document* doc = m_frame->document();
    if (!doc)
        return;
        
    HistoryItem* itemToRestore = 0;
    
    switch (loadType()) {
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            break;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
        case FrameLoadTypeRedirect:
        case FrameLoadTypeStandard:
            itemToRestore = m_currentHistoryItem.get(); 
    }
    
    if (!itemToRestore)
        return;

    LOG(Loading, "WebCoreLoading %s: restoring form state from %p", m_frame->tree()->name().string().utf8().data(), itemToRestore);
    doc->setStateForNewFormElements(itemToRestore->documentState());
}

void FrameLoader::gotoAnchor()
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH If CSS target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set (see bug 11321).
    if (!m_URL.hasRef() && !(m_frame->document() && m_frame->document()->getCSSTarget()))
        return;

    String ref = m_URL.ref();
    if (gotoAnchor(ref))
        return;

    // Try again after decoding the ref, based on the document's encoding.
    if (m_decoder)
        gotoAnchor(decodeURLEscapeSequences(ref, m_decoder->encoding()));
}

void FrameLoader::finishedParsing()
{
    if (m_creatingInitialEmptyDocument)
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    // Null-checking the FrameView indicates whether or not we're in the destructor.
    RefPtr<Frame> protector = m_frame->view() ? m_frame : 0;

    m_client->dispatchDidFinishDocumentLoad();

    checkCompleted();

    if (!m_frame->view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame->view()->restoreScrollbar();

    gotoAnchor();
}

void FrameLoader::loadDone()
{
    if (m_frame->document())
        checkCompleted();
}

void FrameLoader::checkCompleted()
{
    // Any frame that hasn't completed yet?
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        if (!child->loader()->m_isComplete)
            return;

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame->document() && m_frame->document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (m_frame->document())
        if (numRequests(m_frame->document()))
            return;

#if USE(LOW_BANDWIDTH_DISPLAY)
    // as switch will be called, don't complete yet
    if (m_frame->document() && m_frame->document()->inLowBandwidthDisplay() && m_needToSwitchOutLowBandwidthDisplay)
        return;
#endif

    // OK, completed.
    m_isComplete = true;

    RefPtr<Frame> protect(m_frame);
    checkCallImplicitClose(); // if we didn't do it before

    // Do not start a redirection timer for subframes here.
    // That is deferred until the parent is completed.
    if (m_scheduledRedirection && !m_frame->tree()->parent())
        startRedirectionTimer();

    completed();
    if (m_frame->page())
        checkLoadComplete();
}

void FrameLoader::checkCompletedTimerFired(Timer<FrameLoader>*)
{
    checkCompleted();
}

void FrameLoader::scheduleCheckCompleted()
{
    if (!m_checkCompletedTimer.isActive())
        m_checkCompletedTimer.startOneShot(0);
}

void FrameLoader::checkLoadCompleteTimerFired(Timer<FrameLoader>*)
{
    if (!m_frame->page())
        return;
    checkLoadComplete();
}

void FrameLoader::scheduleCheckLoadComplete()
{
    if (!m_checkLoadCompleteTimer.isActive())
        m_checkLoadCompleteTimer.startOneShot(0);
}

void FrameLoader::checkCallImplicitClose()
{
    if (m_didCallImplicitClose || !m_frame->document() || m_frame->document()->parsing())
        return;

    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        if (!child->loader()->m_isComplete) // still got a frame running -> too early
            return;

    m_didCallImplicitClose = true;
    m_wasUnloadEventEmitted = false;
    if (m_frame->document())
        m_frame->document()->implicitClose();
}

KURL FrameLoader::baseURL() const
{
    ASSERT(m_frame->document());
    return m_frame->document()->baseURL();
}

String FrameLoader::baseTarget() const
{
    ASSERT(m_frame->document());
    return m_frame->document()->baseTarget();
}

KURL FrameLoader::completeURL(const String& url)
{
    ASSERT(m_frame->document());
    return m_frame->document()->completeURL(url);
}

void FrameLoader::scheduleHTTPRedirection(double delay, const String& url)
{
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
        
    if (!m_frame->page())
        return;

    // We want a new history item if the refresh timeout is > 1 second.
    if (!m_scheduledRedirection || delay <= m_scheduledRedirection->delay)
        scheduleRedirection(new ScheduledRedirection(delay, url, delay <= 1, delay <= 1, false, false));
}

void FrameLoader::scheduleLocationChange(const String& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool wasUserGesture)
{
    if (!m_frame->page())
        return;

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change.
    KURL parsedURL(url);
    if (parsedURL.hasRef() && equalIgnoringRef(m_URL, parsedURL)) {
        changeLocation(url, referrer, lockHistory, lockBackForwardList, wasUserGesture);
        return;
    }

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame changes the location of another frame.
    bool duringLoad = !m_committedFirstRealDocumentLoad;

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (duringLoad) {
        if (m_provisionalDocumentLoader)
            m_provisionalDocumentLoader->stopLoading();
        stopLoading(true);   
    }

    ScheduledRedirection::Type type = duringLoad
        ? ScheduledRedirection::locationChangeDuringLoad : ScheduledRedirection::locationChange;
    scheduleRedirection(new ScheduledRedirection(type, url, referrer, lockHistory, lockBackForwardList, wasUserGesture, false));
}

void FrameLoader::scheduleRefresh(bool wasUserGesture)
{
    if (!m_frame->page())
        return;

    // Handle a location change of a page with no document as a special case.
    // This may happen when a frame requests a refresh of another frame.
    bool duringLoad = !m_frame->document();
    
    // If a refresh was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (duringLoad)
        stopLoading(true);   

    ScheduledRedirection::Type type = duringLoad
        ? ScheduledRedirection::locationChangeDuringLoad : ScheduledRedirection::locationChange;
    scheduleRedirection(new ScheduledRedirection(type, m_URL.string(), m_outgoingReferrer, true, true, wasUserGesture, true));
}

bool FrameLoader::isLocationChange(const ScheduledRedirection& redirection)
{
    switch (redirection.type) {
        case ScheduledRedirection::redirection:
            return false;
        case ScheduledRedirection::historyNavigation:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::locationChangeDuringLoad:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void FrameLoader::scheduleHistoryNavigation(int steps)
{
    if (!m_frame->page())
        return;

    // navigation will always be allowed in the 0 steps case, which is OK because that's supposed to force a reload.
    if (!canGoBackOrForward(steps)) {
        cancelRedirection();
        return;
    }

    // If the steps to navigate is not zero (which needs to force a reload), and if we think the navigation is going to be a fragment load
    // (when the URL we're going to navigate to is the same as the current one, except for the fragment part - but not exactly the same because that's a reload),
    // then we don't need to schedule the navigation.
    if (steps != 0) {
        KURL destination = historyURL(steps);
        // FIXME: This doesn't seem like a reliable way to tell whether or not the load will be a fragment load.
        if (equalIgnoringRef(m_URL, destination) && m_URL != destination) {
            goBackOrForward(steps);
            return;
        }
    }
    
    scheduleRedirection(new ScheduledRedirection(steps));
}

void FrameLoader::goBackOrForward(int distance)
{
    if (distance == 0)
        return;
        
    Page* page = m_frame->page();
    if (!page)
        return;
    BackForwardList* list = page->backForwardList();
    if (!list)
        return;
    
    HistoryItem* item = list->itemAtIndex(distance);
    if (!item) {
        if (distance > 0) {
            int forwardListCount = list->forwardListCount();
            if (forwardListCount > 0) 
                item = list->itemAtIndex(forwardListCount);
        } else {
            int backListCount = list->backListCount();
            if (backListCount > 0)
                item = list->itemAtIndex(-backListCount);
        }
    }
    
    ASSERT(item); // we should not reach this line with an empty back/forward list
    if (item)
        page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void FrameLoader::redirectionTimerFired(Timer<FrameLoader>*)
{
    ASSERT(m_frame->page());

    OwnPtr<ScheduledRedirection> redirection(m_scheduledRedirection.release());

    switch (redirection->type) {
        case ScheduledRedirection::redirection:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::locationChangeDuringLoad:
            changeLocation(redirection->url, redirection->referrer,
                redirection->lockHistory, redirection->lockBackForwardList, redirection->wasUserGesture, redirection->wasRefresh);
            return;
        case ScheduledRedirection::historyNavigation:
            if (redirection->historySteps == 0) {
                // Special case for go(0) from a frame -> reload only the frame
                urlSelected(m_URL, "", 0, redirection->lockHistory, redirection->lockBackForwardList, redirection->wasUserGesture);
                return;
            }
            // go(i!=0) from a frame navigates into the history of the frame only,
            // in both IE and NS (but not in Mozilla). We can't easily do that.
            goBackOrForward(redirection->historySteps);
            return;
    }

    ASSERT_NOT_REACHED();
}

/*
    In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    The item that was the target of the user's navigation is designated as the "targetItem".  
    When this method is called with doClip=YES we're able to create the whole tree except for the target's children, 
    which will be loaded in the future.  That part of the tree will be filled out as the child loads are committed.
*/
void FrameLoader::loadURLIntoChildFrame(const KURL& url, const String& referer, Frame* childFrame)
{
    ASSERT(childFrame);
    HistoryItem* parentItem = currentHistoryItem();
    FrameLoadType loadType = this->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirect;

    KURL workingURL = url;
    
    // If we're moving in the backforward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    if (parentItem && parentItem->children().size() && isBackForwardLoadType(loadType)) {
        HistoryItem* childItem = parentItem->childItemWithName(childFrame->tree()->name());
        if (childItem) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            workingURL = KURL(childItem->originalURLString());
            childLoadType = loadType;
            childFrame->loader()->setProvisionalHistoryItem(childItem);
        }
    }

    RefPtr<Archive> subframeArchive = activeDocumentLoader()->popArchiveForSubframe(childFrame->tree()->name());
    
    if (subframeArchive)
        childFrame->loader()->loadArchive(subframeArchive.release());
    else
        childFrame->loader()->loadURL(workingURL, referer, String(), childLoadType, 0, 0);
}

void FrameLoader::loadArchive(PassRefPtr<Archive> prpArchive)
{
    RefPtr<Archive> archive = prpArchive;
    
    ArchiveResource* mainResource = archive->mainResource();
    ASSERT(mainResource);
    if (!mainResource)
        return;
        
    SubstituteData substituteData(mainResource->data(), mainResource->mimeType(), mainResource->textEncoding(), KURL());
    
    ResourceRequest request(mainResource->url());
#if PLATFORM(MAC)
    request.applyWebArchiveHackForMail();
#endif

    RefPtr<DocumentLoader> documentLoader = m_client->createDocumentLoader(request, substituteData);
    documentLoader->addAllArchiveResources(archive.get());
    load(documentLoader.get());
}

String FrameLoader::encoding() const
{
    if (m_encodingWasChosenByUser && !m_encoding.isEmpty())
        return m_encoding;
    if (m_decoder && m_decoder->encoding().isValid())
        return m_decoder->encoding().name();
    Settings* settings = m_frame->settings();
    return settings ? settings->defaultTextEncodingName() : String();
}

bool FrameLoader::gotoAnchor(const String& name)
{
    ASSERT(m_frame->document());

    if (!m_frame->document()->haveStylesheetsLoaded()) {
        m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(true);
        return false;
    }

    m_frame->document()->setGotoAnchorNeededAfterStylesheetsLoad(false);

    Element* anchorNode = m_frame->document()->findAnchor(name);

#if ENABLE(SVG)
    if (m_frame->document()->isSVGDocument()) {
        if (name.startsWith("xpointer(")) {
            // We need to parse the xpointer reference here
        } else if (name.startsWith("svgView(")) {
            RefPtr<SVGSVGElement> svg = static_cast<SVGDocument*>(m_frame->document())->rootElement();
            if (!svg->currentView()->parseViewSpec(name))
                return false;
            svg->setUseCurrentView(true);
        } else {
            if (anchorNode && anchorNode->hasTagName(SVGNames::viewTag)) {
                RefPtr<SVGViewElement> viewElement = anchorNode->hasTagName(SVGNames::viewTag) ? static_cast<SVGViewElement*>(anchorNode) : 0;
                if (viewElement.get()) {
                    RefPtr<SVGSVGElement> svg = static_cast<SVGSVGElement*>(SVGLocatable::nearestViewportElement(viewElement.get()));
                    svg->inheritViewAttributes(viewElement.get());
                }
            }
        }
        // FIXME: need to decide which <svg> to focus on, and zoom to that one
        // FIXME: need to actually "highlight" the viewTarget(s)
    }
#endif

    m_frame->document()->setCSSTarget(anchorNode); // Setting to null will clear the current target.
  
    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    if (m_frame->document()) {
        m_frame->document()->updateRendering();
        // Only do a layout if changes have occurred that make it necessary.
        if (m_frame->view() && m_frame->contentRenderer() && m_frame->contentRenderer()->needsLayout())
            m_frame->view()->layout();
    }
  
    // Scroll nested layers and frames to reveal the anchor.
    // Align to the top and to the closest side (this matches other browsers).
    RenderObject* renderer;
    IntRect rect;
    if (!anchorNode)
        renderer = m_frame->document()->renderer(); // top of document
    else {
        renderer = anchorNode->renderer();
        rect = anchorNode->getRect();
    }
    if (renderer)
        renderer->enclosingLayer()->scrollRectToVisible(rect, true, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);

    return true;
}

bool FrameLoader::requestObject(RenderPart* renderer, const String& url, const AtomicString& frameName,
    const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;

#if USE(LOW_BANDWIDTH_DISPLAY)
    // don't care object during low bandwidth display
    if (frame()->document()->inLowBandwidthDisplay()) {
        m_needToSwitchOutLowBandwidthDisplay = true;
        return false;
    }
#endif

    KURL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    bool useFallback;
    if (shouldUsePlugin(completedURL, mimeType, renderer->hasFallbackContent(), useFallback)) {
        Settings* settings = m_frame->settings();
        if (!settings || !settings->arePluginsEnabled() || 
            (!settings->isJavaEnabled() && MIMETypeRegistry::isJavaAppletMIMEType(mimeType)))
            return false;
        return loadPlugin(renderer, completedURL, mimeType, paramNames, paramValues, useFallback);
    }

    ASSERT(renderer->node()->hasTagName(objectTag) || renderer->node()->hasTagName(embedTag));
    HTMLPlugInElement* element = static_cast<HTMLPlugInElement*>(renderer->node());
    
    // FIXME: OK to always make a new frame? When does the old frame get removed?
    return loadSubframe(element, completedURL, frameName, m_outgoingReferrer);
}

bool FrameLoader::shouldUsePlugin(const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    if (m_client->shouldUsePluginDocument(mimeType)) {
        useFallback = false;
        return true;
    }

    // Allow other plug-ins to win over QuickTime because if the user has installed a plug-in that
    // can handle TIFF (which QuickTime can also handle) they probably intended to override QT.
    if (m_frame->page() && (mimeType == "image/tiff" || mimeType == "image/tif" || mimeType == "image/x-tiff")) {
        const PluginData* pluginData = m_frame->page()->pluginData();
        String pluginName = pluginData ? pluginData->pluginNameForMimeType(mimeType) : String();
        if (!pluginName.isEmpty() && !pluginName.contains("QuickTime", false)) 
            return true;
    }
        
    ObjectContentType objectType = m_client->objectContentType(url, mimeType);
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentNone && hasFallback;
    return objectType == ObjectContentNone || objectType == ObjectContentNetscapePlugin || objectType == ObjectContentOtherPlugin;
}

bool FrameLoader::loadPlugin(RenderPart* renderer, const KURL& url, const String& mimeType, 
    const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    Widget* widget = 0;

    if (renderer && !useFallback) {
        Element* pluginElement = 0;
        if (renderer->node() && renderer->node()->isElementNode())
            pluginElement = static_cast<Element*>(renderer->node());

        if (!canLoad(url, String(), frame()->document())) {
            FrameLoader::reportLocalLoadFailed(m_frame, url.string());
            return false;
        }

        widget = m_client->createPlugin(IntSize(renderer->contentWidth(), renderer->contentHeight()), 
                                        pluginElement, url, paramNames, paramValues, mimeType,
                                        m_frame->document()->isPluginDocument());
        if (widget) {
            renderer->setWidget(widget);
            m_containsPlugIns = true;
        }
    }

    return widget != 0;
}

void FrameLoader::clearRecordedFormValues()
{
    m_formAboutToBeSubmitted = 0;
    m_formValuesAboutToBeSubmitted.clear();
}

void FrameLoader::setFormAboutToBeSubmitted(PassRefPtr<HTMLFormElement> element)
{
    m_formAboutToBeSubmitted = element;
}

void FrameLoader::recordFormValue(const String& name, const String& value)
{
    m_formValuesAboutToBeSubmitted.set(name, value);
}

void FrameLoader::parentCompleted()
{
    if (m_scheduledRedirection && !m_redirectionTimer.isActive())
        startRedirectionTimer();
}

String FrameLoader::outgoingReferrer() const
{
    return m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    if (m_frame->document())
        return m_frame->document()->securityOrigin()->toString();

    return SecurityOrigin::createEmpty()->toString();
}

Frame* FrameLoader::opener()
{
    return m_opener;
}

void FrameLoader::setOpener(Frame* opener)
{
    if (m_opener)
        m_opener->loader()->m_openedFrames.remove(m_frame);
    if (opener)
        opener->loader()->m_openedFrames.add(m_frame);
    m_opener = opener;

    if (m_frame->document()) {
        m_frame->document()->initSecurityContext();
        m_frame->domWindow()->setSecurityOrigin(m_frame->document()->securityOrigin());
    }
}

bool FrameLoader::openedByDOM() const
{
    return m_openedByDOM;
}

void FrameLoader::setOpenedByDOM()
{
    m_openedByDOM = true;
}

void FrameLoader::handleFallbackContent()
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElement*>(owner)->renderFallbackContent();
}

void FrameLoader::provisionalLoadStarted()
{
    Page* page = m_frame->page();
    
    // this is used to update the current history item
    // in the event of a navigation aytime during loading
    m_navigationDuringLoad = false;
    if (page) {
        Document *document = page->mainFrame()->document();
        m_navigationDuringLoad = !page->mainFrame()->loader()->isComplete() || (document && document->processingLoadEvent());
    }
    
    m_firstLayoutDone = false;
    cancelRedirection(true);
    m_client->provisionalLoadStarted();
}

bool FrameLoader::userGestureHint()
{
    Frame* frame = m_frame->tree()->top();
    if (!frame->script()->isEnabled())
        return true; // If JavaScript is disabled, a user gesture must have initiated the navigation.
    return frame->script()->processingUserGesture(); // FIXME: Use pageIsProcessingUserGesture.
}

void FrameLoader::didNotOpenURL(const KURL& url)
{
    if (m_submittedFormURL == url)
        m_submittedFormURL = KURL();
}

void FrameLoader::resetMultipleFormSubmissionProtection()
{
    m_submittedFormURL = KURL();
}

void FrameLoader::setEncoding(const String& name, bool userChosen)
{
    if (!m_workingURL.isEmpty())
        receivedFirstData();
    m_encoding = name;
    m_encodingWasChosenByUser = userChosen;
}

void FrameLoader::addData(const char* bytes, int length)
{
    ASSERT(m_workingURL.isEmpty());
    ASSERT(m_frame->document());
    ASSERT(m_frame->document()->parsing());
    write(bytes, length);
}

bool FrameLoader::canCachePageContainingThisFrame()
{
    return m_documentLoader
        && m_documentLoader->mainDocumentError().isNull()
        && !m_frame->tree()->childCount()
        // FIXME: If we ever change this so that frames with plug-ins will be cached,
        // we need to make sure that we don't cache frames that have outstanding NPObjects
        // (objects created by the plug-in). Since there is no way to pause/resume a Netscape plug-in,
        // they would need to be destroyed and then recreated, and there is no way that we can recreate
        // the right NPObjects. See <rdar://problem/5197041> for more information.
        && !m_containsPlugIns
        && !m_URL.protocolIs("https")
        && m_frame->document()
        && !m_frame->document()->hasWindowEventListener(eventNames().unloadEvent)
#if ENABLE(DATABASE)
        && !m_frame->document()->hasOpenDatabases()
#endif
        && !m_frame->document()->usingGeolocation()
        && m_currentHistoryItem
        && !isQuickRedirectComing()
        && !m_documentLoader->isLoadingInAPISense()
        && !m_documentLoader->isStopping()
        && m_frame->document()->canSuspendActiveDOMObjects()
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        // FIXME: We should investigating caching frames that have an associated
        // application cache. <rdar://problem/5917899> tracks that work.
        && !m_documentLoader->applicationCache()
        && !m_documentLoader->candidateApplicationCacheGroup()
#endif
        && m_client->canCachePage()
        ;
}

bool FrameLoader::canCachePage()
{
#ifndef NDEBUG
    logCanCachePageDecision();
#endif
    
    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    // FIXME: <rdar://problem/4886592> - We should work out the complexities of caching pages with frames as they
    // are the most interesting pages on the web, and often those that would benefit the most from caching!
    FrameLoadType loadType = this->loadType();
    
    return !m_frame->tree()->parent()
        && canCachePageContainingThisFrame()
        && m_frame->page()
        && m_frame->page()->backForwardList()->enabled()
        && m_frame->page()->backForwardList()->capacity() > 0
        && m_frame->page()->settings()->usesPageCache()
        && loadType != FrameLoadTypeReload 
        && loadType != FrameLoadTypeReloadFromOrigin
        && loadType != FrameLoadTypeSame
        ;
}

#ifndef NDEBUG
static String& pageCacheLogPrefix(int indentLevel)
{
    static int previousIndent = -1;
    DEFINE_STATIC_LOCAL(String, prefix, ());
    
    if (indentLevel != previousIndent) {    
        previousIndent = indentLevel;
        prefix.truncate(0);
        for (int i = 0; i < previousIndent; ++i)
            prefix += "    ";
    }
    
    return prefix;
}

static void pageCacheLog(const String& prefix, const String& message)
{
    LOG(PageCache, "%s%s", prefix.utf8().data(), message.utf8().data());
}

#define PCLOG(...) pageCacheLog(pageCacheLogPrefix(indentLevel), String::format(__VA_ARGS__))

void FrameLoader::logCanCachePageDecision()
{
    // Only bother logging for main frames that have actually loaded and have content.
    if (m_creatingInitialEmptyDocument)
        return;
    KURL currentURL = m_documentLoader ? m_documentLoader->url() : KURL();
    if (currentURL.isEmpty())
        return;

    int indentLevel = 0;
    PCLOG("--------\n Determining if page can be cached:");

    bool cannotCache = !logCanCacheFrameDecision(1);
        
    FrameLoadType loadType = this->loadType();
    do {
        if (m_frame->tree()->parent())
            { PCLOG("   -Frame has a parent frame"); cannotCache = true; }
        if (!m_frame->page()) {
            PCLOG("   -There is no Page object");
            cannotCache = true;
            break;
        }
        if (!m_frame->page()->backForwardList()->enabled())
            { PCLOG("   -The back/forward list is disabled"); cannotCache = true; }
        if (!(m_frame->page()->backForwardList()->capacity() > 0))
            { PCLOG("   -The back/forward list has a 0 capacity"); cannotCache = true; }
        if (!m_frame->page()->settings()->usesPageCache())
            { PCLOG("   -Page settings says b/f cache disabled"); cannotCache = true; }
        if (loadType == FrameLoadTypeReload)
            { PCLOG("   -Load type is: Reload"); cannotCache = true; }
        if (loadType == FrameLoadTypeReloadFromOrigin)
            { PCLOG("   -Load type is: Reload from origin"); cannotCache = true; }
        if (loadType == FrameLoadTypeSame)
            { PCLOG("   -Load type is: Same"); cannotCache = true; }
    } while (false);
    
    PCLOG(cannotCache ? " Page CANNOT be cached\n--------" : " Page CAN be cached\n--------");
}

bool FrameLoader::logCanCacheFrameDecision(int indentLevel)
{
    // Only bother logging for frames that have actually loaded and have content.
    if (m_creatingInitialEmptyDocument)
        return false;
    KURL currentURL = m_documentLoader ? m_documentLoader->url() : KURL();
    if (currentURL.isEmpty())
        return false;

    PCLOG("+---");
    KURL newURL = m_provisionalDocumentLoader ? m_provisionalDocumentLoader->url() : KURL();
    if (!newURL.isEmpty())
        PCLOG(" Determining if frame can be cached navigating from (%s) to (%s):", currentURL.string().utf8().data(), newURL.string().utf8().data());
    else
        PCLOG(" Determining if subframe with URL (%s) can be cached:", currentURL.string().utf8().data());
        
    bool cannotCache = false;

    do {
        if (!m_documentLoader) {
            PCLOG("   -There is no DocumentLoader object");
            cannotCache = true;
            break;
        }
        if (!m_documentLoader->mainDocumentError().isNull())
            { PCLOG("   -Main document has an error"); cannotCache = true; }
        if (m_frame->tree()->childCount())
            { PCLOG("   -Frame has child frames"); cannotCache = true; }
        if (m_containsPlugIns)
            { PCLOG("   -Frame contains plugins"); cannotCache = true; }
        if (m_URL.protocolIs("https"))
            { PCLOG("   -Frame is HTTPS"); cannotCache = true; }
        if (!m_frame->document()) {
            PCLOG("   -There is no Document object");
            cannotCache = true;
            break;
        }
        if (m_frame->document()->hasWindowEventListener(eventNames().unloadEvent))
            { PCLOG("   -Frame has an unload event listener"); cannotCache = true; }
#if ENABLE(DATABASE)
        if (m_frame->document()->hasOpenDatabases())
            { PCLOG("   -Frame has open database handles"); cannotCache = true; }
#endif
        if (m_frame->document()->usingGeolocation())
            { PCLOG("   -Frame uses Geolocation"); cannotCache = true; }
        if (!m_currentHistoryItem)
            { PCLOG("   -No current history item"); cannotCache = true; }
        if (isQuickRedirectComing())
            { PCLOG("   -Quick redirect is coming"); cannotCache = true; }
        if (m_documentLoader->isLoadingInAPISense())
            { PCLOG("   -DocumentLoader is still loading in API sense"); cannotCache = true; }
        if (m_documentLoader->isStopping())
            { PCLOG("   -DocumentLoader is in the middle of stopping"); cannotCache = true; }
        if (!m_frame->document()->canSuspendActiveDOMObjects())
            { PCLOG("   -The document cannot suspect its active DOM Objects"); cannotCache = true; }
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        if (m_documentLoader->applicationCache())
            { PCLOG("   -The DocumentLoader has an active application cache"); cannotCache = true; }
        if (m_documentLoader->candidateApplicationCacheGroup())
            { PCLOG("   -The DocumentLoader has a candidateApplicationCacheGroup"); cannotCache = true; }
#endif
        if (!m_client->canCachePage())
            { PCLOG("   -The client says this frame cannot be cached"); cannotCache = true; }
    } while (false);

    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        if (!child->loader()->logCanCacheFrameDecision(indentLevel + 1))
            cannotCache = true;
    
    PCLOG(cannotCache ? " Frame CANNOT be cached" : " Frame CAN be cached");
    PCLOG("+---");

    return !cannotCache;
}
#endif

void FrameLoader::updatePolicyBaseURL()
{
    if (m_frame->tree()->parent() && m_frame->tree()->parent()->document())
        setPolicyBaseURL(m_frame->tree()->parent()->document()->policyBaseURL());
    else
        setPolicyBaseURL(m_URL);
}

void FrameLoader::setPolicyBaseURL(const KURL& url)
{
    if (m_frame->document())
        m_frame->document()->setPolicyBaseURL(url);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->setPolicyBaseURL(url);
}

// This does the same kind of work that didOpenURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::scrollToAnchor(const KURL& url)
{
    m_URL = url;
    updateHistoryForAnchorScroll();

    // If we were in the autoscroll/panScroll mode we want to stop it before following the link to the anchor
    m_frame->eventHandler()->stopAutoscrollTimer();
    started();
    gotoAnchor();

    // It's important to model this as a load that starts and immediately finishes.
    // Otherwise, the parent frame may think we never finished loading.
    m_isComplete = false;
    checkCompleted();
}

bool FrameLoader::isComplete() const
{
    return m_isComplete;
}

void FrameLoader::scheduleRedirection(ScheduledRedirection* redirection)
{
    ASSERT(m_frame->page());

    stopRedirectionTimer();
    m_scheduledRedirection.set(redirection);
    if (!m_isComplete && redirection->type != ScheduledRedirection::redirection)
        completed();
    if (m_isComplete || redirection->type != ScheduledRedirection::redirection)
        startRedirectionTimer();
}

void FrameLoader::startRedirectionTimer()
{
    ASSERT(m_frame->page());
    ASSERT(m_scheduledRedirection);

    m_redirectionTimer.stop();
    m_redirectionTimer.startOneShot(m_scheduledRedirection->delay);

    switch (m_scheduledRedirection->type) {
        case ScheduledRedirection::redirection:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::locationChangeDuringLoad:
            clientRedirected(KURL(m_scheduledRedirection->url),
                m_scheduledRedirection->delay,
                currentTime() + m_redirectionTimer.nextFireInterval(),
                m_scheduledRedirection->lockHistory,
                m_scheduledRedirection->lockBackForwardList,
                m_isExecutingJavaScriptFormAction);
            return;
        case ScheduledRedirection::historyNavigation:
            // Don't report history navigations.
            return;
    }
    ASSERT_NOT_REACHED();
}

void FrameLoader::stopRedirectionTimer()
{
    if (!m_redirectionTimer.isActive())
        return;

    m_redirectionTimer.stop();

    if (m_scheduledRedirection) {
        switch (m_scheduledRedirection->type) {
            case ScheduledRedirection::redirection:
            case ScheduledRedirection::locationChange:
            case ScheduledRedirection::locationChangeDuringLoad:
                clientRedirectCancelledOrFinished(m_cancellingWithLoadInProgress);
                return;
            case ScheduledRedirection::historyNavigation:
                // Don't report history navigations.
                return;
        }
        ASSERT_NOT_REACHED();
    }
}

void FrameLoader::completed()
{
    RefPtr<Frame> protect(m_frame);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->parentCompleted();
    if (Frame* parent = m_frame->tree()->parent())
        parent->loader()->checkCompleted();
    submitFormAgain();
}

void FrameLoader::started()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->parent())
        frame->loader()->m_isComplete = false;
}

bool FrameLoader::containsPlugins() const 
{ 
    return m_containsPlugIns;
}

void FrameLoader::prepareForLoadStart()
{
    if (Page* page = m_frame->page())
        page->progress()->progressStarted(m_frame);
    m_client->dispatchDidStartProvisionalLoad();
}

void FrameLoader::setupForReplace()
{
    setState(FrameStateProvisional);
    m_provisionalDocumentLoader = m_documentLoader;
    m_documentLoader = 0;
    detachChildren();
}

void FrameLoader::setupForReplaceByMIMEType(const String& newMIMEType)
{
    activeDocumentLoader()->setupForReplaceByMIMEType(newMIMEType);
}

void FrameLoader::loadFrameRequestWithFormAndValues(const FrameLoadRequest& request, bool lockHistory, bool lockBackForwardList, Event* event,
    HTMLFormElement* submitForm, const HashMap<String, String>& formValues)
{
    RefPtr<FormState> formState;
    if (submitForm)
        formState = FormState::create(submitForm, formValues, m_frame);
    
    KURL url = request.resourceRequest().url();

    String referrer;
    String argsReferrer = request.resourceRequest().httpReferrer();
    if (!argsReferrer.isEmpty())
        referrer = argsReferrer;
    else
        referrer = m_outgoingReferrer;

    ASSERT(frame()->document());
    if (url.protocolIs("file")) {
        if (!canLoad(url, String(), frame()->document()) && !canLoad(url, referrer)) {
            FrameLoader::reportLocalLoadFailed(m_frame, url.string());
            return;
        }
    }

    if (shouldHideReferrer(url, referrer))
        referrer = String();
    
    FrameLoadType loadType;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        loadType = FrameLoadTypeReload;
    else if (lockHistory && lockBackForwardList)
        loadType = FrameLoadTypeRedirect;
    else
        loadType = FrameLoadTypeStandard;    

    if (request.resourceRequest().httpMethod() == "POST")
        loadPostRequest(request.resourceRequest(), referrer, request.frameName(), loadType, event, formState.release());
    else
        loadURL(request.resourceRequest().url(), referrer, request.frameName(), loadType, event, formState.release());

    Frame* targetFrame = findFrameForNavigation(request.frameName());
    if (targetFrame && targetFrame != m_frame)
        if (Page* page = targetFrame->page())
            page->chrome()->focus();
}

void FrameLoader::loadURL(const KURL& newURL, const String& referrer, const String& frameName, FrameLoadType newLoadType,
    Event* event, PassRefPtr<FormState> prpFormState)
{
    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;
    
    ResourceRequest request(newURL);
    if (!referrer.isEmpty()) {
        request.setHTTPReferrer(referrer);
        RefPtr<SecurityOrigin> referrerOrigin = SecurityOrigin::createFromString(referrer);
        addHTTPOriginIfNeeded(request, referrerOrigin->toString());
    }
    addExtraFieldsToRequest(request, newLoadType, true, event || isFormSubmission);
    if (newLoadType == FrameLoadTypeReload || newLoadType == FrameLoadTypeReloadFromOrigin)
        request.setCachePolicy(ReloadIgnoringCacheData);

    ASSERT(newLoadType != FrameLoadTypeSame);

    NavigationAction action(newURL, newLoadType, isFormSubmission, event);

    if (!frameName.isEmpty()) {
        if (Frame* targetFrame = findFrameForNavigation(frameName))
            targetFrame->loader()->loadURL(newURL, referrer, String(), newLoadType, event, formState);
        else
            checkNewWindowPolicy(action, request, formState, frameName);
        return;
    }

    RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;

    bool sameURL = shouldTreatURLAsSameAsCurrent(newURL);
    
    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (shouldScrollToAnchor(isFormSubmission, newLoadType, newURL)) {
        oldDocumentLoader->setTriggeringAction(action);
        stopPolicyCheck();
        checkNavigationPolicy(request, oldDocumentLoader.get(), formState,
            callContinueFragmentScrollAfterNavigationPolicy, this);
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        bool isRedirect = m_quickRedirectComing;
        loadWithNavigationAction(request, action, newLoadType, formState);
        if (isRedirect) {
            m_quickRedirectComing = false;
            if (m_provisionalDocumentLoader)
                m_provisionalDocumentLoader->setIsClientRedirect(true);
        } else if (sameURL)
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            m_loadType = FrameLoadTypeSame;
    }
}

void FrameLoader::load(const ResourceRequest& request)
{
    load(request, SubstituteData());
}

void FrameLoader::load(const ResourceRequest& request, const SubstituteData& substituteData)
{
    if (m_inStopAllLoaders)
        return;
        
    // FIXME: is this the right place to reset loadType? Perhaps this should be done after loading is finished or aborted.
    m_loadType = FrameLoadTypeStandard;
    load(m_client->createDocumentLoader(request, substituteData).get());
}

void FrameLoader::load(const ResourceRequest& request, const String& frameName)
{
    if (frameName.isEmpty()) {
        load(request);
        return;
    }

    Frame* frame = findFrameForNavigation(frameName);
    if (frame) {
        frame->loader()->load(request);
        return;
    }

    checkNewWindowPolicy(NavigationAction(request.url(), NavigationTypeOther), request, 0, frameName);
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, const NavigationAction& action, FrameLoadType type, PassRefPtr<FormState> formState)
{
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, SubstituteData());

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
    } else
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
        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }

    loadWithDocumentLoader(newDocumentLoader, type, 0);
}

void FrameLoader::loadWithDocumentLoader(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> prpFormState)
{
    ASSERT(m_client->hasWebView());

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT(m_frame->view());

    m_policyLoadType = type;
    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;

    const KURL& newURL = loader->request().url();

    if (shouldScrollToAnchor(isFormSubmission, m_policyLoadType, newURL)) {
        RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;
        NavigationAction action(newURL, m_policyLoadType, isFormSubmission);

        oldDocumentLoader->setTriggeringAction(action);
        stopPolicyCheck();
        checkNavigationPolicy(loader->request(), oldDocumentLoader.get(), formState,
            callContinueFragmentScrollAfterNavigationPolicy, this);
    } else {
        if (Frame* parent = m_frame->tree()->parent())
            loader->setOverrideEncoding(parent->loader()->documentLoader()->overrideEncoding());

        stopPolicyCheck();
        setPolicyDocumentLoader(loader);

        checkNavigationPolicy(loader->request(), loader, formState,
            callContinueLoadAfterNavigationPolicy, this);
    }
}

bool FrameLoader::canLoad(const KURL& url, const String& referrer, const Document* doc)
{
    // We can always load any URL that isn't considered local (e.g. http URLs)
    if (!shouldTreatURLAsLocal(url.string()))
        return true;

    // If we were provided a document, we let its local file policy dictate the result,
    // otherwise we allow local loads only if the supplied referrer is also local.
    if (doc)
        return doc->securityOrigin()->canLoadLocalResources();
    else if (!referrer.isEmpty())
        return shouldTreatURLAsLocal(referrer);
    else
        return false;
}

void FrameLoader::reportLocalLoadFailed(Frame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, "Not allowed to load local resource: " + url, 0, String());
}

bool FrameLoader::shouldHideReferrer(const KURL& url, const String& referrer)
{
    bool referrerIsSecureURL = protocolIs(referrer, "https");
    bool referrerIsWebURL = referrerIsSecureURL || protocolIs(referrer, "http");

    if (!referrerIsWebURL)
        return true;

    if (!referrerIsSecureURL)
        return false;

    bool URLIsSecureURL = url.protocolIs("https");

    return !URLIsSecureURL;
}

const ResourceRequest& FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->originalRequest();
}

void FrameLoader::receivedData(const char* data, int length)
{
    activeDocumentLoader()->receivedData(data, length);
}

void FrameLoader::handleUnimplementablePolicy(const ResourceError& error)
{
    m_delegateIsHandlingUnimplementablePolicy = true;
    m_client->dispatchUnableToImplementPolicy(error);
    m_delegateIsHandlingUnimplementablePolicy = false;
}

void FrameLoader::cannotShowMIMEType(const ResourceResponse& response)
{
    handleUnimplementablePolicy(m_client->cannotShowMIMETypeError(response));
}

ResourceError FrameLoader::interruptionForPolicyChangeError(const ResourceRequest& request)
{
    return m_client->interruptForPolicyChangeError(request);
}

void FrameLoader::checkNavigationPolicy(const ResourceRequest& newRequest, NavigationPolicyDecisionFunction function, void* argument)
{
    checkNavigationPolicy(newRequest, activeDocumentLoader(), 0, function, argument);
}

void FrameLoader::checkContentPolicy(const String& MIMEType, ContentPolicyDecisionFunction function, void* argument)
{
    ASSERT(activeDocumentLoader());
    
    // Always show content with valid substitute data.
    if (activeDocumentLoader()->substituteData().isValid()) {
        function(argument, PolicyUse);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    Settings* settings = m_frame->settings();
    if (settings && settings->forceFTPDirectoryListings() && MIMEType == "application/x-ftp-directory") {
        function(argument, PolicyUse);
        return;
    }
#endif

    m_policyCheck.set(function, argument);
    m_client->dispatchDecidePolicyForMIMEType(&FrameLoader::continueAfterContentPolicy,
        MIMEType, activeDocumentLoader()->request());
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(DocumentLoader* docLoader)
{
    KURL unreachableURL = docLoader->unreachableURL();

    if (unreachableURL.isEmpty())
        return false;

    if (!isBackForwardLoadType(m_policyLoadType))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    DocumentLoader* compareDocumentLoader = 0;
    if (m_delegateIsDecidingNavigationPolicy || m_delegateIsHandlingUnimplementablePolicy)
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
    KURL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        request.setURL(unreachableURL);

    request.setCachePolicy(ReturnCacheDataElseLoad);

    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, SubstituteData());
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

    ResourceRequest initialRequest = m_documentLoader->request();

    // Replace error-page URL with the URL we were trying to reach.
    KURL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        initialRequest.setURL(unreachableURL);
    
    // Create a new document loader for the reload, this will become m_documentLoader eventually,
    // but first it has to be the "policy" document loader, and then the "provisional" document loader.
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(initialRequest, SubstituteData());

    ResourceRequest& request = loader->request();

    // FIXME: We don't have a mechanism to revalidate the main resource without reloading at the moment.
    request.setCachePolicy(ReloadIgnoringCacheData);

    // If we're about to re-post, set up action so the application can warn the user.
    if (request.httpMethod() == "POST")
        loader->setTriggeringAction(NavigationAction(request.url(), NavigationTypeFormResubmitted));

    loader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    loadWithDocumentLoader(loader.get(), endToEndReload ? FrameLoadTypeReloadFromOrigin : FrameLoadTypeReload, 0);
}

static bool canAccessAncestor(const SecurityOrigin* activeSecurityOrigin, Frame* targetFrame)
{
    // targetFrame can be NULL when we're trying to navigate a top-level frame
    // that has a NULL opener.
    if (!targetFrame)
        return false;

    for (Frame* ancestorFrame = targetFrame; ancestorFrame; ancestorFrame = ancestorFrame->tree()->parent()) {
        Document* ancestorDocument = ancestorFrame->document();
        if (!ancestorDocument)
            return true;

        const SecurityOrigin* ancestorSecurityOrigin = ancestorDocument->securityOrigin();
        if (activeSecurityOrigin->canAccess(ancestorSecurityOrigin))
            return true;
    }

    return false;
}

bool FrameLoader::shouldAllowNavigation(Frame* targetFrame) const
{
    // The navigation change is safe if the active frame is:
    //   - in the same security origin as the target or one of the target's
    //     ancestors.
    //
    // Or the target frame is:
    //   - a top-level frame in the frame hierarchy and the active frame can
    //     navigate the target frame's opener per above.

    if (!targetFrame)
        return true;

    // Performance optimization.
    if (m_frame == targetFrame)
        return true;

    // Let a frame navigate the top-level window that contains it.  This is
    // important to allow because it lets a site "frame-bust" (escape from a
    // frame created by another web site).
    if (targetFrame == m_frame->tree()->top())
        return true;

    Document* activeDocument = m_frame->document();
    ASSERT(activeDocument);
    const SecurityOrigin* activeSecurityOrigin = activeDocument->securityOrigin();

    // For top-level windows, check the opener.
    if (!targetFrame->tree()->parent() && canAccessAncestor(activeSecurityOrigin, targetFrame->loader()->opener()))
        return true;

    // In general, check the frame's ancestors.
    if (canAccessAncestor(activeSecurityOrigin, targetFrame))
        return true;

    Settings* settings = targetFrame->settings();
    if (settings && !settings->privateBrowsingEnabled()) {
        Document* targetDocument = targetFrame->document();
        // FIXME: this error message should contain more specifics of why the navigation change is not allowed.
        String message = String::format("Unsafe JavaScript attempt to initiate a navigation change for frame with URL %s from frame with URL %s.\n",
            targetDocument->url().string().utf8().data(), activeDocument->url().string().utf8().data());

        // FIXME: should we print to the console of the activeFrame as well?
        targetFrame->domWindow()->console()->addMessage(JSMessageSource, ErrorMessageLevel, message, 1, String());
    }
    
    return false;
}

void FrameLoader::stopLoadingSubframes()
{
    for (RefPtr<Frame> child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopAllLoaders();
}

void FrameLoader::stopAllLoaders()
{
    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;

    m_inStopAllLoaders = true;

    stopPolicyCheck();

    stopLoadingSubframes();
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();

    setProvisionalDocumentLoader(0);
    
    if (m_documentLoader)
        m_documentLoader->clearArchiveResources();

    m_inStopAllLoaders = false;    
}

void FrameLoader::stopForUserCancel(bool deferCheckLoadComplete)
{
    stopAllLoaders();
    
    if (deferCheckLoadComplete)
        scheduleCheckLoadComplete();
    else if (m_frame->page())
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
    return docLoader->isLoadingMainResource() || docLoader->isLoadingSubresources() || docLoader->isLoadingPlugIns();
}

bool FrameLoader::frameHasLoaded() const
{
    return m_committedFirstRealDocumentLoad || (m_provisionalDocumentLoader && !m_creatingInitialEmptyDocument); 
}

void FrameLoader::setDocumentLoader(DocumentLoader* loader)
{
    if (!loader && !m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    m_client->prepareForDataSourceReplacement();
    detachChildren();
    if (m_documentLoader)
        m_documentLoader->detachFromFrame();

    m_documentLoader = loader;
}

DocumentLoader* FrameLoader::documentLoader() const
{
    return m_documentLoader.get();
}

void FrameLoader::setPolicyDocumentLoader(DocumentLoader* loader)
{
    if (m_policyDocumentLoader == loader)
        return;

    ASSERT(m_frame);
    if (loader)
        loader->setFrame(m_frame);
    if (m_policyDocumentLoader
            && m_policyDocumentLoader != m_provisionalDocumentLoader
            && m_policyDocumentLoader != m_documentLoader)
        m_policyDocumentLoader->detachFromFrame();

    m_policyDocumentLoader = loader;
}

DocumentLoader* FrameLoader::policyDocumentLoader() const
{
    return m_policyDocumentLoader.get();
}
   
DocumentLoader* FrameLoader::provisionalDocumentLoader() const
{
    return m_provisionalDocumentLoader.get();
}

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
}

FrameState FrameLoader::state() const
{
    return m_state;
}

double FrameLoader::timeOfLastCompletedLoad()
{
    return storedTimeOfLastCompletedLoad;
}

void FrameLoader::setState(FrameState newState)
{    
    m_state = newState;
    
    if (newState == FrameStateProvisional)
        provisionalLoadStarted();
    else if (newState == FrameStateComplete) {
        frameLoadCompleted();
        storedTimeOfLastCompletedLoad = currentTime();
        if (m_documentLoader)
            m_documentLoader->stopRecordingResponses();
    }
}

void FrameLoader::clearProvisionalLoad()
{
    setProvisionalDocumentLoader(0);
    if (Page* page = m_frame->page())
        page->progress()->progressCompleted(m_frame);
    setState(FrameStateComplete);
}

void FrameLoader::markLoadComplete()
{
    setState(FrameStateComplete);
}

void FrameLoader::commitProvisionalLoad(PassRefPtr<CachedPage> prpCachedPage)
{
    RefPtr<CachedPage> cachedPage = prpCachedPage;
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;

    LOG(Loading, "WebCoreLoading %s: About to commit provisional load from previous URL %s", m_frame->tree()->name().string().utf8().data(), m_URL.string().utf8().data());

    // Check to see if we need to cache the page we are navigating away from into the back/forward cache.
    // We are doing this here because we know for sure that a new page is about to be loaded.
    if (canCachePage() && !m_currentHistoryItem->isInPageCache())
        cachePageForHistoryItem(m_currentHistoryItem.get());
    
    if (m_loadType != FrameLoadTypeReplace)
        closeOldDataSources();
    
    if (!cachedPage && !m_creatingInitialEmptyDocument)
        m_client->makeRepresentation(pdl.get());
    
    transitionToCommitted(cachedPage);
    
    // Call clientRedirectCancelledOrFinished() here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (m_sentRedirectNotification)
        clientRedirectCancelledOrFinished(false);
    
    if (cachedPage && cachedPage->document()) {
        open(*cachedPage);
        cachedPage->clear();
    } else {        
        KURL url = pdl->substituteData().responseURL();
        if (url.isEmpty())
            url = pdl->url();
        if (url.isEmpty())
            url = pdl->responseURL();
        if (url.isEmpty())
            url = blankURL();

        didOpenURL(url);
    }

    LOG(Loading, "WebCoreLoading %s: Finished committing provisional load to URL %s", m_frame->tree()->name().string().utf8().data(), m_URL.string().utf8().data());

    opened();
}

void FrameLoader::transitionToCommitted(PassRefPtr<CachedPage> cachedPage)
{
    ASSERT(m_client->hasWebView());
    ASSERT(m_state == FrameStateProvisional);

    if (m_state != FrameStateProvisional)
        return;

    m_client->setCopiesOnScroll();
    updateHistoryForCommit();

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
    setState(FrameStateCommittedPage);

    // Handle adding the URL to the back/forward list.
    DocumentLoader* dl = m_documentLoader.get();
    String ptitle = dl->title(); 

    switch (m_loadType) {
        case FrameLoadTypeForward:
        case FrameLoadTypeBack:
        case FrameLoadTypeIndexedBackForward:
            if (Page* page = m_frame->page())
                if (page->backForwardList()) {
                    updateHistoryForBackForwardNavigation();

                    // Create a document view for this document, or used the cached view.
                    if (cachedPage) {
                        DocumentLoader* cachedDocumentLoader = cachedPage->documentLoader();
                        ASSERT(cachedDocumentLoader);
                        cachedDocumentLoader->setFrame(m_frame);
                        m_client->transitionToCommittedFromCachedFrame(cachedPage->cachedMainFrame());
                        
                    } else
                        m_client->transitionToCommittedForNewPage();
                }
            break;

        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            updateHistoryForReload();
            m_client->transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeStandard:
            updateHistoryForStandardLoad();
#ifndef BUILDING_ON_TIGER
            // This code was originally added for a Leopard performance imporvement. We decided to 
            // ifdef it to fix correctness issues on Tiger documented in <rdar://problem/5441823>.
            if (m_frame->view())
                m_frame->view()->setScrollbarsSuppressed(true);
#endif
            m_client->transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeRedirect:
            updateHistoryForRedirectWithLockedHistory();
            m_client->transitionToCommittedForNewPage();
            break;

        // FIXME Remove this check when dummy ds is removed (whatever "dummy ds" is).
        // An exception should be thrown if we're in the FrameLoadTypeUninitialized state.
        default:
            ASSERT_NOT_REACHED();
    }

    m_responseMIMEType = dl->responseMIMEType();

    // Tell the client we've committed this URL.
    ASSERT(m_frame->view());

    if (m_creatingInitialEmptyDocument)
        return;

    m_committedFirstRealDocumentLoad = true;
    
    // For non-cached HTML pages, these methods are called in FrameLoader::begin.
    if (cachedPage || !m_client->hasHTMLView()) {
        dispatchDidCommitLoad(); 
            
        // If we have a title let the WebView know about it. 
        if (!ptitle.isNull()) 
            m_client->dispatchDidReceiveTitle(ptitle);         
    }
}

void FrameLoader::clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress)
{
    // Note that -webView:didCancelClientRedirectForFrame: is called on the frame load delegate even if
    // the redirect succeeded.  We should either rename this API, or add a new method, like
    // -webView:didFinishClientRedirectForFrame:
    m_client->dispatchDidCancelClientRedirect();

    if (!cancelWithLoadInProgress)
        m_quickRedirectComing = false;

    m_sentRedirectNotification = false;
}

void FrameLoader::clientRedirected(const KURL& url, double seconds, double fireDate, bool lockHistory, bool lockBackForwardList, bool isJavaScriptFormAction)
{
    m_client->dispatchWillPerformClientRedirect(url, seconds, fireDate);
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    m_sentRedirectNotification = true;
    
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    m_quickRedirectComing = (lockHistory && lockBackForwardList) && m_documentLoader && !isJavaScriptFormAction;
}

#if ENABLE(WML)
void FrameLoader::setForceReloadWmlDeck(bool reload)
{
    m_forceReloadWmlDeck = reload;
}
#endif

bool FrameLoader::shouldReload(const KURL& currentURL, const KURL& destinationURL)
{
#if ENABLE(WML)
    // As for WML deck, sometimes it's supposed to be reloaded even if the same URL with fragment
    if (m_forceReloadWmlDeck)
        return true;
#endif

    // This function implements the rule: "Don't reload if navigating by fragment within
    // the same URL, but do reload if going to a new URL or to the same URL with no
    // fragment identifier at all."
    if (!destinationURL.hasRef())
        return true;
    return !equalIgnoringRef(currentURL, destinationURL);
}

void FrameLoader::closeOldDataSources()
{
    // FIXME: Is it important for this traversal to be postorder instead of preorder?
    // If so, add helpers for postorder traversal, and use them. If not, then lets not
    // use a recursive algorithm here.
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->closeOldDataSources();
    
    if (m_documentLoader)
        m_client->dispatchWillClose();

    m_client->setMainFrameDocumentReady(false); // stop giving out the actual DOMDocument to observers
}

void FrameLoader::open(CachedPage& cachedPage)
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->page()->mainFrame() == m_frame);

    cancelRedirection();

    // We still have to close the previous part page.
    closeURL();

    m_isComplete = false;
    
    // Don't re-emit the load event.
    m_didCallImplicitClose = true;
    
    // Delete old status bar messages (if it _was_ activated on last URL).
    if (m_frame->script()->isEnabled()) {
        m_frame->setJSStatusBarText(String());
        m_frame->setJSDefaultStatusBarText(String());
    }

    KURL url = cachedPage.url();

    if ((url.protocolIs("http") || url.protocolIs("https")) && !url.host().isEmpty() && url.path().isEmpty())
        url.setPath("/");
    
    m_URL = url;
    m_workingURL = url;

    started();

    clear();

    Document* document = cachedPage.document();
    ASSERT(document);
    document->setInPageCache(false);

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_outgoingReferrer = url.string();

    FrameView* view = cachedPage.view();
    if (view)
        view->setWasScrolledByUser(false);
    m_frame->setView(view);
    
    m_frame->setDocument(document);
    m_frame->setDOMWindow(cachedPage.domWindow());
    m_frame->domWindow()->setURL(document->url());
    m_frame->domWindow()->setSecurityOrigin(document->securityOrigin());

    m_decoder = document->decoder();

    updatePolicyBaseURL();

    cachedPage.restore(m_frame->page());

    // It is necessary to update any platform script objects after restoring the
    // cached page.
    m_frame->script()->updatePlatformScriptObjects();

    checkCompleted();
}

bool FrameLoader::isStopping() const
{
    return activeDocumentLoader()->isStopping();
}

void FrameLoader::finishedLoading()
{
    // Retain because the stop may release the last reference to it.
    RefPtr<Frame> protect(m_frame);

    RefPtr<DocumentLoader> dl = activeDocumentLoader();
    dl->finishedLoading();
    if (!dl->mainDocumentError().isNull() || !dl->frameLoader())
        return;
    dl->setPrimaryLoadComplete(true);
    m_client->dispatchDidLoadMainResource(dl.get());
    checkLoadComplete();
}

bool FrameLoader::isHostedByObjectElement() const
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    return owner && owner->hasTagName(objectTag);
}

bool FrameLoader::isLoadingMainFrame() const
{
    Page* page = m_frame->page();
    return page && m_frame == page->mainFrame();
}

bool FrameLoader::canShowMIMEType(const String& MIMEType) const
{
    return m_client->canShowMIMEType(MIMEType);
}

bool FrameLoader::representationExistsForURLScheme(const String& URLScheme)
{
    return m_client->representationExistsForURLScheme(URLScheme);
}

String FrameLoader::generatedMIMETypeForURLScheme(const String& URLScheme)
{
    return m_client->generatedMIMETypeForURLScheme(URLScheme);
}

void FrameLoader::cancelContentPolicyCheck()
{
    m_client->cancelPolicyCheck();
    m_policyCheck.clear();
}

void FrameLoader::didReceiveServerRedirectForProvisionalLoadForFrame()
{
    m_client->dispatchDidReceiveServerRedirectForProvisionalLoad();
}

void FrameLoader::finishedLoadingDocument(DocumentLoader* loader)
{
    // FIXME: Platforms shouldn't differ here!
#if PLATFORM(WIN) || PLATFORM(CHROMIUM)
    if (m_creatingInitialEmptyDocument)
        return;
#endif
    
    // If loading a webarchive, run through webarchive machinery
    const String& responseMIMEType = loader->responseMIMEType();

    // FIXME: Mac's FrameLoaderClient::finishedLoading() method does work that is required even with Archive loads
    // so we still need to call it.  Other platforms should only call finishLoading for non-archive loads
    // That work should be factored out so this #ifdef can be removed
#if PLATFORM(MAC)
    m_client->finishedLoading(loader);
    if (!ArchiveFactory::isArchiveMimeType(responseMIMEType))
        return;
#else
    if (!ArchiveFactory::isArchiveMimeType(responseMIMEType)) {
        m_client->finishedLoading(loader);
        return;
    }
#endif
        
    RefPtr<Archive> archive(ArchiveFactory::create(loader->mainResourceData().get(), responseMIMEType));
    if (!archive)
        return;

    loader->addAllArchiveResources(archive.get());
    
    ArchiveResource* mainResource = archive->mainResource();
    loader->setParsedArchiveData(mainResource->data());
    continueLoadWithData(mainResource->data(), mainResource->mimeType(), mainResource->textEncoding(), mainResource->url());
}

bool FrameLoader::isReplacing() const
{
    return m_loadType == FrameLoadTypeReplace;
}

void FrameLoader::setReplacing()
{
    m_loadType = FrameLoadTypeReplace;
}

void FrameLoader::revertToProvisional(DocumentLoader* loader)
{
    m_client->revertToProvisionalState(loader);
}

bool FrameLoader::subframeIsLoading() const
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling()) {
        FrameLoader* childLoader = child->loader();
        DocumentLoader* documentLoader = childLoader->documentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->provisionalDocumentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
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
    
CachePolicy FrameLoader::cachePolicy() const
{
    if (m_isComplete)
        return CachePolicyVerify;
    
    if (m_loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;
    
    if (Frame* parentFrame = m_frame->tree()->parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader()->cachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    if (m_loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    return CachePolicyVerify;
}

void FrameLoader::stopPolicyCheck()
{
    m_client->cancelPolicyCheck();
    PolicyCheck check = m_policyCheck;
    m_policyCheck.clear();
    check.cancel();
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT(m_client->hasWebView());

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
            if (Page* page = m_frame->page())
                if (isBackForwardLoadType(loadType()) && m_frame == page->mainFrame())
                    item = m_currentHistoryItem;
                
            bool shouldReset = true;
            if (!pdl->isLoadingInAPISense()) {
                m_delegateIsHandlingProvisionalLoadError = true;
                m_client->dispatchDidFailProvisionalLoad(error);
                m_delegateIsHandlingProvisionalLoadError = false;

                // FIXME: can stopping loading here possibly have any effect, if isLoading is false,
                // which it must be to be in this branch of the if? And is it OK to just do a full-on
                // stopAllLoaders instead of stopLoadingSubframes?
                stopLoadingSubframes();
                pdl->stopLoading();

                // Finish resetting the load state, but only if another load hasn't been started by the
                // delegate callback.
                if (pdl == m_provisionalDocumentLoader)
                    clearProvisionalLoad();
                else if (m_provisionalDocumentLoader) {
                    KURL unreachableURL = m_provisionalDocumentLoader->unreachableURL();
                    if (!unreachableURL.isEmpty() && unreachableURL == pdl->request().url())
                        shouldReset = false;
                }
            }
            if (shouldReset && item)
                if (Page* page = m_frame->page()) {
                    page->backForwardList()->goToItem(item.get());
                    Settings* settings = m_frame->settings();
                    page->setGlobalHistoryItem((!settings || settings->privateBrowsingEnabled()) ? 0 : item.get());
                }
            return;
        }
        
        case FrameStateCommittedPage: {
            DocumentLoader* dl = m_documentLoader.get();            
            if (!dl || dl->isLoadingInAPISense())
                return;

            markLoadComplete();

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            m_client->forceLayoutForNonHTML();
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if (Page* page = m_frame->page())
                if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin) && page->backForwardList())
                    restoreScrollPositionAndViewState();

            if (m_creatingInitialEmptyDocument || !m_committedFirstRealDocumentLoad)
                return;

            const ResourceError& error = dl->mainDocumentError();
#ifndef NDEBUG
            m_didDispatchDidCommitLoad = false;
#endif
            if (!error.isNull())
                m_client->dispatchDidFailLoad(error);
            else
                m_client->dispatchDidFinishLoad();

            if (Page* page = m_frame->page())
                page->progress()->progressCompleted(m_frame);
            return;
        }
        
        case FrameStateComplete:
            frameLoadCompleted();
            return;
    }

    ASSERT_NOT_REACHED();
}

void FrameLoader::continueAfterContentPolicy(PolicyAction policy)
{
    PolicyCheck check = m_policyCheck;
    m_policyCheck.clear();
    check.call(policy);
}

void FrameLoader::continueLoadAfterWillSubmitForm(PolicyAction)
{
    if (!m_provisionalDocumentLoader)
        return;

    // DocumentLoader calls back to our prepareForLoadStart
    m_provisionalDocumentLoader->prepareForLoadStart();
    
    // The load might be cancelled inside of prepareForLoadStart(), nulling out the m_provisionalDocumentLoader, 
    // so we need to null check it again.
    if (!m_provisionalDocumentLoader)
        return;

    DocumentLoader* activeDocLoader = activeDocumentLoader();
    if (activeDocLoader && activeDocLoader->isLoadingMainResource())
        return;

    m_provisionalDocumentLoader->setLoadingFromCachedPage(false);

    unsigned long identifier = 0;

    if (Page* page = m_frame->page()) {
        identifier = page->progress()->createUniqueIdentifier();
        dispatchAssignIdentifierToInitialRequest(identifier, m_provisionalDocumentLoader.get(), m_provisionalDocumentLoader->originalRequest());
    }

    if (!m_provisionalDocumentLoader->startLoadingMainResource(identifier))
        m_provisionalDocumentLoader->updateLoading();
}

void FrameLoader::didFirstLayout()
{
    if (Page* page = m_frame->page())
        if (isBackForwardLoadType(m_loadType) && page->backForwardList())
            restoreScrollPositionAndViewState();

    m_firstLayoutDone = true;
    m_client->dispatchDidFirstLayout();
}

void FrameLoader::didFirstVisuallyNonEmptyLayout()
{
    m_client->dispatchDidFirstVisuallyNonEmptyLayout();
}

void FrameLoader::frameLoadCompleted()
{
    // Note: Can be called multiple times.

    m_client->frameLoadCompleted();

    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    setPreviousHistoryItem(0);

    // After a canceled provisional load, firstLayoutDone is false.
    // Reset it to true if we're displaying a page.
    if (m_documentLoader)
        m_firstLayoutDone = true;
}

bool FrameLoader::firstLayoutDone() const
{
    return m_firstLayoutDone;
}

bool FrameLoader::isQuickRedirectComing() const
{
    return m_quickRedirectComing;
}

void FrameLoader::detachChildren()
{
    // FIXME: Is it really necessary to do this in reverse order?
    Frame* previous;
    for (Frame* child = m_frame->tree()->lastChild(); child; child = previous) {
        previous = child->tree()->previousSibling();
        child->loader()->detachFromParent();
    }
}

void FrameLoader::recursiveCheckLoadComplete()
{
    Vector<RefPtr<Frame>, 10> frames;
    
    for (RefPtr<Frame> frame = m_frame->tree()->firstChild(); frame; frame = frame->tree()->nextSibling())
        frames.append(frame);
    
    unsigned size = frames.size();
    for (unsigned i = 0; i < size; i++)
        frames[i]->loader()->recursiveCheckLoadComplete();
    
    checkLoadCompleteForThisFrame();
}

// Called every time a resource is completely loaded, or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(m_client->hasWebView());
    
    // FIXME: Always traversing the entire frame tree is a bit inefficient, but 
    // is currently needed in order to null out the previous history item for all frames.
    if (Page* page = m_frame->page())
        page->mainFrame()->loader()->recursiveCheckLoadComplete();
}

int FrameLoader::numPendingOrLoadingRequests(bool recurse) const
{
    if (!recurse)
        return numRequests(m_frame->document());

    int count = 0;
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame))
        count += numRequests(frame->document());
    return count;
}

void FrameLoader::submitForm(const FrameLoadRequest& request, Event* event, bool lockHistory, bool lockBackForwardList)
{
    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.
    Frame* target = m_frame->tree()->find(request.frameName());
    if (m_frame->tree()->isDescendantOf(target)) {
        if (m_submittedFormURL == request.resourceRequest().url())
            return;
        m_submittedFormURL = request.resourceRequest().url();
    }

    loadFrameRequestWithFormAndValues(request, lockHistory, lockBackForwardList, event, m_formAboutToBeSubmitted.get(), m_formValuesAboutToBeSubmitted);

    clearRecordedFormValues();
}

String FrameLoader::userAgent(const KURL& url) const
{
    return m_client->userAgent(url);
}

void FrameLoader::tokenizerProcessedData()
{
//    ASSERT(m_frame->page());
//    ASSERT(m_frame->document());

    checkCompleted();
}

void FrameLoader::handledOnloadEvents()
{
    m_client->dispatchDidHandleOnloadEvents();
}

void FrameLoader::frameDetached()
{
    stopAllLoaders();
    if (Document* document = m_frame->document())
        document->stopActiveDOMObjects();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    RefPtr<Frame> protect(m_frame);

    closeURL();
    stopAllLoaders();
    saveScrollPositionAndViewStateToItem(currentHistoryItem());
    detachChildren();

    if (Page* page = m_frame->page())
        page->inspectorController()->frameDetachedFromParent(m_frame);

    m_client->detachedFromParent2();
    setDocumentLoader(0);
    m_client->detachedFromParent3();
    if (Frame* parent = m_frame->tree()->parent()) {
        parent->tree()->removeChild(m_frame);
        parent->loader()->scheduleCheckCompleted();
    } else {
        m_frame->setView(0);
        m_frame->pageDestroyed();
    }
}
    
void FrameLoader::addExtraFieldsToSubresourceRequest(ResourceRequest& request)
{
    addExtraFieldsToRequest(request, m_loadType, false, false);
}

void FrameLoader::addExtraFieldsToMainResourceRequest(ResourceRequest& request)
{
    addExtraFieldsToRequest(request, m_loadType, true, false);
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request, FrameLoadType loadType, bool mainResource, bool cookiePolicyURLFromRequest)
{
    applyUserAgent(request);
    
    if (loadType == FrameLoadTypeReload) {
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "max-age=0");
    } else if (loadType == FrameLoadTypeReloadFromOrigin) {
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "no-cache");
        request.setHTTPHeaderField("Pragma", "no-cache");
    }
    
    // Don't set the cookie policy URL if it's already been set.
    if (request.mainDocumentURL().isEmpty()) {
        if (mainResource && (isLoadingMainFrame() || cookiePolicyURLFromRequest))
            request.setMainDocumentURL(request.url());
        else if (Page* page = m_frame->page())
            request.setMainDocumentURL(page->mainFrame()->loader()->url());
    }
    
    if (mainResource)
        request.setHTTPAccept("application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");

    // Make sure we send the Origin header.
    addHTTPOriginIfNeeded(request, String());

    // Always try UTF-8. If that fails, try frame encoding (if any) and then the default.
    // For a newly opened frame with an empty URL, encoding() should not be used, because this methods asks decoder, which uses ISO-8859-1.
    Settings* settings = m_frame->settings();
    request.setResponseContentDispositionEncodingFallbackArray("UTF-8", m_URL.isEmpty() ? m_encoding : encoding(), settings ? settings->defaultTextEncodingName() : String());
}

void FrameLoader::addHTTPOriginIfNeeded(ResourceRequest& request, String origin)
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
        origin = SecurityOrigin::createEmpty()->toString();
    }

    request.setHTTPOrigin(origin);
}

void FrameLoader::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    if (ArchiveFactory::isArchiveMimeType(loader->response().mimeType()))
        return;
    m_client->committedLoad(loader, data, length);
}

void FrameLoader::loadPostRequest(const ResourceRequest& inRequest, const String& referrer, const String& frameName, FrameLoadType loadType, Event* event, PassRefPtr<FormState> prpFormState)
{
    RefPtr<FormState> formState = prpFormState;

    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    // Previously when this method was reached, the original FrameLoadRequest had been deconstructed to build a 
    // bunch of parameters that would come in here and then be built back up to a ResourceRequest.  In case
    // any caller depends on the immutability of the original ResourceRequest, I'm rebuilding a ResourceRequest
    // from scratch as it did all along.
    const KURL& url = inRequest.url();
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
    addExtraFieldsToRequest(workingResourceRequest, loadType, true, true);

    NavigationAction action(url, loadType, true, event);

    if (!frameName.isEmpty()) {
        if (Frame* targetFrame = findFrameForNavigation(frameName))
            targetFrame->loader()->loadWithNavigationAction(workingResourceRequest, action, loadType, formState.release());
        else
            checkNewWindowPolicy(action, workingResourceRequest, formState.release(), frameName);
    } else
        loadWithNavigationAction(workingResourceRequest, action, loadType, formState.release());    
}

void FrameLoader::loadEmptyDocumentSynchronously()
{
    ResourceRequest request(KURL(""));
    load(request);
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    // Since this is a subresource, we can load any URL (we ignore the return value).
    // But we still want to know whether we should hide the referrer or not, so we call the canLoad method.
    String referrer = m_outgoingReferrer;
    if (shouldHideReferrer(request.url(), referrer))
        referrer = String();
    
    ResourceRequest initialRequest = request;
    initialRequest.setTimeoutInterval(10);
    
    if (initialRequest.isConditional())
        initialRequest.setCachePolicy(ReloadIgnoringCacheData);
    else
        initialRequest.setCachePolicy(documentLoader()->request().cachePolicy());
    
    if (!referrer.isEmpty())
        initialRequest.setHTTPReferrer(referrer);
    addHTTPOriginIfNeeded(initialRequest, outgoingOrigin());

    if (Page* page = m_frame->page())
        initialRequest.setMainDocumentURL(page->mainFrame()->loader()->documentLoader()->request().url());
    initialRequest.setHTTPUserAgent(client()->userAgent(request.url()));

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        ApplicationCacheResource* resource;
        if (documentLoader()->shouldLoadResourceFromApplicationCache(newRequest, resource)) {
            if (resource) {
                response = resource->response();
                data.append(resource->data()->data(), resource->data()->size());
            } else
                error = cannotShowURLError(newRequest);
        } else {
#endif
            ResourceHandle::loadResourceSynchronously(newRequest, error, response, data, m_frame);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
            // If normal loading results in a redirect to a resource with another origin (indicative of a captive portal), or a 4xx or 5xx status code or equivalent,
            // or if there were network errors (but not if the user canceled the download), then instead get, from the cache, the resource of the fallback entry
            // corresponding to the matched namespace.
            if ((!error.isNull() && !error.isCancellation())
                 || response.httpStatusCode() / 100 == 4 || response.httpStatusCode() / 100 == 5
                 || !protocolHostAndPortAreEqual(newRequest.url(), response.url())) {
                if (documentLoader()->getApplicationCacheFallbackResource(newRequest, resource)) {
                    response = resource->response();
                    data.clear();
                    data.append(resource->data()->data(), resource->data()->size());
                }
            }
        }
#endif
    }
    
    sendRemainingDelegateMessages(identifier, response, data.size(), error);
    return identifier;
}

void FrameLoader::assignIdentifierToInitialRequest(unsigned long identifier, const ResourceRequest& clientRequest)
{
    return dispatchAssignIdentifierToInitialRequest(identifier, activeDocumentLoader(), clientRequest);
}

void FrameLoader::willSendRequest(ResourceLoader* loader, ResourceRequest& clientRequest, const ResourceResponse& redirectResponse)
{
    applyUserAgent(clientRequest);
    dispatchWillSendRequest(loader->documentLoader(), loader->identifier(), clientRequest, redirectResponse);
}

void FrameLoader::didReceiveResponse(ResourceLoader* loader, const ResourceResponse& r)
{
    activeDocumentLoader()->addResponse(r);
    
    if (Page* page = m_frame->page())
        page->progress()->incrementProgress(loader->identifier(), r);
    dispatchDidReceiveResponse(loader->documentLoader(), loader->identifier(), r);
}

void FrameLoader::didReceiveData(ResourceLoader* loader, const char* data, int length, int lengthReceived)
{
    if (Page* page = m_frame->page())
        page->progress()->incrementProgress(loader->identifier(), data, length);
    dispatchDidReceiveContentLength(loader->documentLoader(), loader->identifier(), lengthReceived);
}

void FrameLoader::didFailToLoad(ResourceLoader* loader, const ResourceError& error)
{
    if (Page* page = m_frame->page())
        page->progress()->completeProgress(loader->identifier());
    if (!error.isNull())
        m_client->dispatchDidFailLoading(loader->documentLoader(), loader->identifier(), error);
}

const ResourceRequest& FrameLoader::originalRequest() const
{
    return activeDocumentLoader()->originalRequestCopy();
}

void FrameLoader::receivedMainResourceError(const ResourceError& error, bool isComplete)
{
    // Retain because the stop may release the last reference to it.
    RefPtr<Frame> protect(m_frame);

    RefPtr<DocumentLoader> loader = activeDocumentLoader();
    
    if (isComplete) {
        // FIXME: Don't want to do this if an entirely new load is going, so should check
        // that both data sources on the frame are either this or nil.
        stop();
        if (m_client->shouldFallBack(error))
            handleFallbackContent();
    }
    
    if (m_state == FrameStateProvisional && m_provisionalDocumentLoader) {
        KURL failedURL = m_provisionalDocumentLoader->originalRequestCopy().url();
        didNotOpenURL(failedURL);
            
        // We might have made a page cache item, but now we're bailing out due to an error before we ever
        // transitioned to the new page (before WebFrameState == commit).  The goal here is to restore any state
        // so that the existing view (that wenever got far enough to replace) can continue being used.
        invalidateCurrentItemCachedPage();
        
        // Call clientRedirectCancelledOrFinished here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect. The frame load delegate may have saved some state about
        // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:. Since we are definitely
        // not going to use this provisional resource, as it was cancelled, notify the frame load delegate that the redirect
        // has ended.
        if (m_sentRedirectNotification)
            clientRedirectCancelledOrFinished(false);
    }
    
    
    loader->mainReceivedError(error, isComplete);
}

void FrameLoader::callContinueFragmentScrollAfterNavigationPolicy(void* argument,
    const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue)
{
    FrameLoader* loader = static_cast<FrameLoader*>(argument);
    loader->continueFragmentScrollAfterNavigationPolicy(request, shouldContinue);
}

void FrameLoader::continueFragmentScrollAfterNavigationPolicy(const ResourceRequest& request, bool shouldContinue)
{
    bool isRedirect = m_quickRedirectComing || m_policyLoadType == FrameLoadTypeRedirect;
    m_quickRedirectComing = false;

    if (!shouldContinue)
        return;

    KURL url = request.url();
    
    m_documentLoader->replaceRequestURLForAnchorScroll(url);
    if (!isRedirect && !shouldTreatURLAsSameAsCurrent(url)) {
        // NB: must happen after _setURL, since we add based on the current request.
        // Must also happen before we openURL and displace the scroll position, since
        // adding the BF item will save away scroll state.
        
        // NB2:  If we were loading a long, slow doc, and the user anchor nav'ed before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.

        addHistoryItemForFragmentScroll();
    }
    
    scrollToAnchor(url);
    
    if (!isRedirect)
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state.
        checkLoadComplete();
 
    m_client->dispatchDidChangeLocationWithinPage();
    m_client->didFinishLoad();
}

bool FrameLoader::shouldScrollToAnchor(bool isFormSubmission, FrameLoadType loadType, const KURL& url)
{
    // Should we do anchor navigation within the existing content?

    // We don't do this if we are submitting a form, explicitly reloading,
    // currently displaying a frameset, or if the URL does not have a fragment.
    // These rules were originally based on what KHTML was doing in KHTMLPart::openURL.

    // FIXME: What about load types other than Standard and Reload?

    return !isFormSubmission
        && loadType != FrameLoadTypeReload
        && loadType != FrameLoadTypeReloadFromOrigin
        && loadType != FrameLoadTypeSame
        && !shouldReload(this->url(), url)
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && (!m_frame->document() || !m_frame->document()->isFrameSet());
}

void FrameLoader::opened()
{
    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        updateHistoryForClientRedirect();

    if (m_documentLoader->isLoadingFromCachedPage()) {
        m_frame->document()->documentDidBecomeActive();
        
        // Force a layout to update view size and thereby update scrollbars.
        m_client->forceLayout();

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
            sendRemainingDelegateMessages(identifier, response, static_cast<int>(response.expectedContentLength()), error);
        }
        
        pageCache()->remove(m_currentHistoryItem.get());

        m_documentLoader->setPrimaryLoadComplete(true);

        // FIXME: Why only this frame and not parent frames?
        checkLoadCompleteForThisFrame();
    }
}

void FrameLoader::checkNewWindowPolicy(const NavigationAction& action, const ResourceRequest& request,
    PassRefPtr<FormState> formState, const String& frameName)
{
    m_policyCheck.set(request, formState, frameName,
        callContinueLoadAfterNewWindowPolicy, this);
    m_client->dispatchDecidePolicyForNewWindowAction(&FrameLoader::continueAfterNewWindowPolicy,
        action, request, formState, frameName);
}

void FrameLoader::continueAfterNewWindowPolicy(PolicyAction policy)
{
    PolicyCheck check = m_policyCheck;
    m_policyCheck.clear();

    switch (policy) {
        case PolicyIgnore:
            check.clearRequest();
            break;
        case PolicyDownload:
            m_client->startDownload(check.request());
            check.clearRequest();
            break;
        case PolicyUse:
            break;
    }

    check.call(policy == PolicyUse);
}

void FrameLoader::checkNavigationPolicy(const ResourceRequest& request, DocumentLoader* loader,
    PassRefPtr<FormState> formState, NavigationPolicyDecisionFunction function, void* argument)
{
    NavigationAction action = loader->triggeringAction();
    if (action.isEmpty()) {
        action = NavigationAction(request.url(), NavigationTypeOther);
        loader->setTriggeringAction(action);
    }
        
    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if (equalIgnoringHeaderFields(request, loader->lastCheckedRequest()) || (!request.isNull() && request.url().isEmpty())) {
        function(argument, request, 0, true);
        loader->setLastCheckedRequest(request);
        return;
    }
    
    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    if (loader->substituteData().isValid() && !loader->substituteData().failingURL().isEmpty()) {
        if (isBackForwardLoadType(m_policyLoadType))
            m_policyLoadType = FrameLoadTypeReload;
        function(argument, request, 0, true);
        return;
    }
    
    loader->setLastCheckedRequest(request);

    m_policyCheck.set(request, formState.get(), function, argument);

    m_delegateIsDecidingNavigationPolicy = true;
    m_client->dispatchDecidePolicyForNavigationAction(&FrameLoader::continueAfterNavigationPolicy,
        action, request, formState);
    m_delegateIsDecidingNavigationPolicy = false;
}

void FrameLoader::continueAfterNavigationPolicy(PolicyAction policy)
{
    PolicyCheck check = m_policyCheck;
    m_policyCheck.clear();

    bool shouldContinue = policy == PolicyUse;
    
    switch (policy) {
        case PolicyIgnore:
            check.clearRequest();
            break;
        case PolicyDownload:
            m_client->startDownload(check.request());
            check.clearRequest();
            break;
        case PolicyUse: {
            ResourceRequest request(check.request());
            
            if (!m_client->canHandleRequest(request)) {
                handleUnimplementablePolicy(m_client->cannotShowURLError(check.request()));
                check.clearRequest();
                shouldContinue = false;
            }
            break;
        }
    }

    check.call(shouldContinue);
}

void FrameLoader::callContinueLoadAfterNavigationPolicy(void* argument,
    const ResourceRequest& request, PassRefPtr<FormState> formState, bool shouldContinue)
{
    FrameLoader* loader = static_cast<FrameLoader*>(argument);
    loader->continueLoadAfterNavigationPolicy(request, formState, shouldContinue);
}

void FrameLoader::continueLoadAfterNavigationPolicy(const ResourceRequest&, PassRefPtr<FormState> formState, bool shouldContinue)
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT(m_policyDocumentLoader || !m_provisionalDocumentLoader->unreachableURL().isEmpty());

    bool isTargetItem = m_provisionalHistoryItem ? m_provisionalHistoryItem->isTargetItem() : false;

    // Two reasons we can't continue:
    //    1) Navigation policy delegate said we can't so request is nil. A primary case of this 
    //       is the user responding Cancel to the form repost nag sheet.
    //    2) User responded Cancel to an alert popped up by the before unload event handler.
    // The "before unload" event handler runs only for the main frame.
    bool canContinue = shouldContinue && (!isLoadingMainFrame() || m_frame->shouldClose());

    if (!canContinue) {
        // If we were waiting for a quick redirect, but the policy delegate decided to ignore it, then we 
        // need to report that the client redirect was cancelled.
        if (m_quickRedirectComing)
            clientRedirectCancelledOrFinished(false);

        setPolicyDocumentLoader(0);

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || isLoadingMainFrame()) && isBackForwardLoadType(m_policyLoadType))
            if (Page* page = m_frame->page()) {
                Frame* mainFrame = page->mainFrame();
                if (HistoryItem* resetItem = mainFrame->loader()->m_currentHistoryItem.get()) {
                    page->backForwardList()->goToItem(resetItem);
                    Settings* settings = m_frame->settings();
                    page->setGlobalHistoryItem((!settings || settings->privateBrowsingEnabled()) ? 0 : resetItem);
                }
            }
        return;
    }

    FrameLoadType type = m_policyLoadType;
    stopAllLoaders();
    
    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load. 
    if (!m_frame->page())
        return;
        
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_loadType = type;
    setState(FrameStateProvisional);

    setPolicyDocumentLoader(0);

    if (isBackForwardLoadType(type) && loadProvisionalItemFromCachedPage())
        return;

    if (formState)
        m_client->dispatchWillSubmitForm(&FrameLoader::continueLoadAfterWillSubmitForm, formState);
    else
        continueLoadAfterWillSubmitForm();
}


void FrameLoader::callContinueLoadAfterNewWindowPolicy(void* argument,
    const ResourceRequest& request, PassRefPtr<FormState> formState, const String& frameName, bool shouldContinue)
{
    FrameLoader* loader = static_cast<FrameLoader*>(argument);
    loader->continueLoadAfterNewWindowPolicy(request, formState, frameName, shouldContinue);
}

void FrameLoader::continueLoadAfterNewWindowPolicy(const ResourceRequest& request,
    PassRefPtr<FormState> formState, const String& frameName, bool shouldContinue)
{
    if (!shouldContinue)
        return;

    RefPtr<Frame> frame = m_frame;
    RefPtr<Frame> mainFrame = m_client->dispatchCreatePage();
    if (!mainFrame)
        return;

    if (frameName != "_blank")
        mainFrame->tree()->setName(frameName);

    mainFrame->loader()->setOpenedByDOM();
    mainFrame->loader()->m_client->dispatchShow();
    mainFrame->loader()->setOpener(frame.get());
    mainFrame->loader()->loadWithNavigationAction(request, NavigationAction(), FrameLoadTypeStandard, formState);
}

void FrameLoader::sendRemainingDelegateMessages(unsigned long identifier, const ResourceResponse& response, int length, const ResourceError& error)
{    
    if (!response.isNull())
        dispatchDidReceiveResponse(m_documentLoader.get(), identifier, response);
    
    if (length > 0)
        dispatchDidReceiveContentLength(m_documentLoader.get(), identifier, length);
    
    if (error.isNull())
        dispatchDidFinishLoading(m_documentLoader.get(), identifier);
    else
        m_client->dispatchDidFailLoading(m_documentLoader.get(), identifier, error);
}

void FrameLoader::requestFromDelegate(ResourceRequest& request, unsigned long& identifier, ResourceError& error)
{
    ASSERT(!request.isNull());

    identifier = 0;
    if (Page* page = m_frame->page()) {
        identifier = page->progress()->createUniqueIdentifier();
        dispatchAssignIdentifierToInitialRequest(identifier, m_documentLoader.get(), request);
    }

    ResourceRequest newRequest(request);
    dispatchWillSendRequest(m_documentLoader.get(), identifier, newRequest, ResourceResponse());

    if (newRequest.isNull())
        error = cancelledError(request);
    else
        error = ResourceError();

    request = newRequest;
}

void FrameLoader::loadedResourceFromMemoryCache(const CachedResource* resource)
{
    Page* page = m_frame->page();
    if (!page)
        return;

    page->inspectorController()->didLoadResourceFromMemoryCache(m_documentLoader.get(), resource);

    if (!resource->sendResourceLoadCallbacks() || m_documentLoader->haveToldClientAboutLoad(resource->url()))
        return;

    if (!page->areMemoryCacheClientCallsEnabled()) {
        m_documentLoader->recordMemoryCacheLoadForFutureClientNotification(resource->url());
        m_documentLoader->didTellClientAboutLoad(resource->url());
        return;
    }

    ResourceRequest request(resource->url());
    if (m_client->dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, resource->response(), resource->encodedSize())) {
        m_documentLoader->didTellClientAboutLoad(resource->url());
        return;
    }

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    sendRemainingDelegateMessages(identifier, resource->response(), resource->encodedSize(), error);
}

void FrameLoader::applyUserAgent(ResourceRequest& request)
{
    String userAgent = client()->userAgent(request.url());
    ASSERT(!userAgent.isNull());
    request.setHTTPUserAgent(userAgent);
}

bool FrameLoader::canGoBackOrForward(int distance) const
{
    if (Page* page = m_frame->page()) {
        if (distance == 0)
            return true;
        if (distance > 0 && distance <= page->backForwardList()->forwardListCount())
            return true;
        if (distance < 0 && -distance <= page->backForwardList()->backListCount())
            return true;
    }
    return false;
}

int FrameLoader::getHistoryLength()
{
    if (Page* page = m_frame->page())
        return page->backForwardList()->backListCount() + 1;
    return 0;
}

KURL FrameLoader::historyURL(int distance)
{
    if (Page* page = m_frame->page()) {
        BackForwardList* list = page->backForwardList();
        HistoryItem* item = list->itemAtIndex(distance);
        if (!item) {
            if (distance > 0) {
                int forwardListCount = list->forwardListCount();
                if (forwardListCount > 0)
                    item = list->itemAtIndex(forwardListCount);
            } else {
                int backListCount = list->backListCount();
                if (backListCount > 0)
                    item = list->itemAtIndex(-backListCount);
            }
        }
        if (item)
            return item->url();
    }
    return KURL();
}

void FrameLoader::addHistoryItemForFragmentScroll()
{
    addBackForwardItemClippedAtTarget(false);
}

bool FrameLoader::loadProvisionalItemFromCachedPage()
{
    RefPtr<CachedPage> cachedPage = pageCache()->get(m_provisionalHistoryItem.get());
    if (!cachedPage || !cachedPage->document())
        return false;
    provisionalDocumentLoader()->loadFromCachedPage(cachedPage.release());
    return true;
}

void FrameLoader::cachePageForHistoryItem(HistoryItem* item)
{
    if (Page* page = m_frame->page()) {
        RefPtr<CachedPage> cachedPage = CachedPage::create(page);
        pageCache()->add(item, cachedPage.release());
    }
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& url) const
{
    if (!m_currentHistoryItem)
        return false;
    return url == m_currentHistoryItem->url() || url == m_currentHistoryItem->originalURL();
}

PassRefPtr<HistoryItem> FrameLoader::createHistoryItem(bool useOriginal)
{
    DocumentLoader* docLoader = documentLoader();
    
    KURL unreachableURL = docLoader ? docLoader->unreachableURL() : KURL();
    
    KURL url;
    KURL originalURL;

    if (!unreachableURL.isEmpty()) {
        url = unreachableURL;
        originalURL = unreachableURL;
    } else {
        originalURL = docLoader ? docLoader->originalURL() : KURL();
        if (useOriginal)
            url = originalURL;
        else if (docLoader)
            url = docLoader->requestURL();
    }

    LOG(History, "WebCoreHistory: Creating item for %s", url.string().ascii().data());
    
    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (url.isEmpty()) 
        url = blankURL();
    if (originalURL.isEmpty())
        originalURL = blankURL();
    
    Frame* parentFrame = m_frame->tree()->parent();
    String parent = parentFrame ? parentFrame->tree()->name() : "";
    String title = docLoader ? docLoader->title() : "";

    RefPtr<HistoryItem> item = HistoryItem::create(url, m_frame->tree()->name(), parent, title);
    item->setOriginalURLString(originalURL.string());

    if (!unreachableURL.isEmpty() || !docLoader || docLoader->response().httpStatusCode() >= 400)
        item->setLastVisitWasFailure(true);

    // Save form state if this is a POST
    if (docLoader) {
        if (useOriginal)
            item->setFormInfoFromRequest(docLoader->originalRequest());
        else
            item->setFormInfoFromRequest(docLoader->request());
    }
    
    // Set the item for which we will save document state
    m_previousHistoryItem = m_currentHistoryItem;
    m_currentHistoryItem = item;
    
    return item.release();
}

void FrameLoader::addBackForwardItemClippedAtTarget(bool doClip)
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (documentLoader()->urlForHistory().isEmpty())
        return;

    Frame* mainFrame = page->mainFrame();
    ASSERT(mainFrame);
    FrameLoader* frameLoader = mainFrame->loader();

    if (!frameLoader->m_didPerformFirstNavigation && page->backForwardList()->entries().size() == 1) {
        frameLoader->m_didPerformFirstNavigation = true;
        m_client->didPerformFirstNavigation();
    }

    RefPtr<HistoryItem> item = frameLoader->createHistoryItemTree(m_frame, doClip);
    LOG(BackForward, "WebCoreBackForward - Adding backforward item %p for frame %s", item.get(), documentLoader()->url().string().ascii().data());
    page->backForwardList()->addItem(item);
}

PassRefPtr<HistoryItem> FrameLoader::createHistoryItemTree(Frame* targetFrame, bool clipAtTarget)
{
    RefPtr<HistoryItem> bfItem = createHistoryItem(m_frame->tree()->parent() ? true : false);
    if (m_previousHistoryItem)
        saveScrollPositionAndViewStateToItem(m_previousHistoryItem.get());
    if (!(clipAtTarget && m_frame == targetFrame)) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        saveDocumentState();
        for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
            FrameLoader* childLoader = child->loader();
            bool hasChildLoaded = childLoader->frameHasLoaded();
            
            // If the child is a frame corresponding to an <object> element that never loaded,
            // we don't want to create a history item, because that causes fallback content
            // to be ignored on reload.
            
            if (!(!hasChildLoaded && childLoader->isHostedByObjectElement()))
                bfItem->addChildItem(childLoader->createHistoryItemTree(targetFrame, clipAtTarget));
        }
    }
    if (m_frame == targetFrame)
        bfItem->setIsTargetItem(true);
    return bfItem;
}

Frame* FrameLoader::findFrameForNavigation(const AtomicString& name)
{
    Frame* frame = m_frame->tree()->find(name);
    if (shouldAllowNavigation(frame))
        return frame;  
    return 0;
}

void FrameLoader::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    if (!item || !m_frame->view())
        return;
        
    item->setScrollPoint(m_frame->view()->scrollPosition());
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling through to the client.
    m_client->saveViewStateToItem(item);
}

/*
 There is a race condition between the layout and load completion that affects restoring the scroll position.
 We try to restore the scroll position at both the first layout and upon load completion.
 
 1) If first layout happens before the load completes, we want to restore the scroll position then so that the
 first time we draw the page is already scrolled to the right place, instead of starting at the top and later
 jumping down.  It is possible that the old scroll position is past the part of the doc laid out so far, in
 which case the restore silent fails and we will fix it in when we try to restore on doc completion.
 2) If the layout happens after the load completes, the attempt to restore at load completion time silently
 fails.  We then successfully restore it when the layout happens.
*/
void FrameLoader::restoreScrollPositionAndViewState()
{
    if (!m_committedFirstRealDocumentLoad)
        return;

    ASSERT(m_currentHistoryItem);
    
    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll or view state to restore!
    if (!m_currentHistoryItem)
        return;
    
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling
    // through to the client. It's currently used only for the PDF view on Mac.
    m_client->restoreViewState();
    
    if (FrameView* view = m_frame->view())
        if (!view->wasScrolledByUser())
            view->setScrollPosition(m_currentHistoryItem->scrollPoint());
}

void FrameLoader::invalidateCurrentItemCachedPage()
{
    // When we are pre-commit, the currentItem is where the pageCache data resides    
    CachedPage* cachedPage = pageCache()->get(m_currentHistoryItem.get());

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    ASSERT(!cachedPage || cachedPage->document() == m_frame->document());
    if (cachedPage && cachedPage->document() == m_frame->document()) {
        cachedPage->document()->setInPageCache(false);
        cachedPage->clear();
    }
    
    if (cachedPage)
        pageCache()->remove(m_currentHistoryItem.get());
}

void FrameLoader::saveDocumentState()
{
    if (m_creatingInitialEmptyDocument)
        return;

    // For a standard page load, we will have a previous item set, which will be used to
    // store the form state.  However, in some cases we will have no previous item, and
    // the current item is the right place to save the state.  One example is when we
    // detach a bunch of frames because we are navigating from a site with frames to
    // another site.  Another is when saving the frame state of a frame that is not the
    // target of the current navigation (if we even decide to save with that granularity).

    // Because of previousItem's "masking" of currentItem for this purpose, it's important
    // that previousItem be cleared at the end of a page transition.  We leverage the
    // checkLoadComplete recursion to achieve this goal.

    HistoryItem* item = m_previousHistoryItem ? m_previousHistoryItem.get() : m_currentHistoryItem.get();
    if (!item)
        return;

    Document* document = m_frame->document();
    ASSERT(document);
    
    if (document && item->isCurrentDocument(document)) {
        LOG(Loading, "WebCoreLoading %s: saving form state to %p", m_frame->tree()->name().string().utf8().data(), item);
        item->setDocumentState(document->formElementsState());
    }
}

// Loads content into this frame, as specified by history item
void FrameLoader::loadItem(HistoryItem* item, FrameLoadType loadType)
{
    if (!m_frame->page())
        return;

    KURL itemURL = item->url();
    KURL itemOriginalURL = item->originalURL();
    KURL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->url();
    RefPtr<FormData> formData = item->formData();

    // Are we navigating to an anchor within the page?
    // Note if we have child frames we do a real reload, since the child frames might not
    // match our current frame structure, or they might not have the right content.  We could
    // check for all that as an additional optimization.
    // We also do not do anchor-style navigation if we're posting a form.
    
#if ENABLE(WML)
    if (!formData && urlsMatchItem(item) && !m_frame->document()->isWMLDocument()) {
#else
    if (!formData && urlsMatchItem(item)) {
#endif
        // Must do this maintenance here, since we don't go through a real page reload
        saveScrollPositionAndViewStateToItem(m_currentHistoryItem.get());

        if (FrameView* view = m_frame->view())
            view->setWasScrolledByUser(false);

        m_currentHistoryItem = item;

        // FIXME: Form state might need to be saved here too.

        // We always call scrollToAnchor here, even if the URL doesn't have an
        // anchor fragment. This is so we'll keep the WebCore Frame's URL up-to-date.
        scrollToAnchor(item->url());
    
        // must do this maintenance here, since we don't go through a real page reload
        restoreScrollPositionAndViewState();
        
        // Fake the URL change by updating the data source's request.  This will no longer
        // be necessary if we do the better fix described above.
        documentLoader()->replaceRequestURLForAnchorScroll(itemURL);

        m_client->dispatchDidChangeLocationWithinPage();
        
        // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
        m_client->didFinishLoad();
    } else {
        // Remember this item so we can traverse any child items as child frames load
        m_provisionalHistoryItem = item;

        bool inPageCache = false;
        
        // Check if we'll be using the page cache.  We only use the page cache
        // if one exists and it is less than _backForwardCacheExpirationInterval
        // seconds old.  If the cache is expired it gets flushed here.
        if (RefPtr<CachedPage> cachedPage = pageCache()->get(item)) {
            double interval = currentTime() - cachedPage->timeStamp();
            
            // FIXME: 1800 should not be hardcoded, it should come from
            // WebKitBackForwardCacheExpirationIntervalKey in WebKit.
            // Or we should remove WebKitBackForwardCacheExpirationIntervalKey.
            if (interval <= 1800) {
                loadWithDocumentLoader(cachedPage->documentLoader(), loadType, 0);   
                inPageCache = true;
            } else {
                LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", m_provisionalHistoryItem->url().string().ascii().data());
                pageCache()->remove(item);
            }
        }
        
        if (!inPageCache) {
            bool addedExtraFields = false;
            ResourceRequest request(itemURL);

            if (!item->referrer().isNull())
                request.setHTTPReferrer(item->referrer());
            
            // If this was a repost that failed the page cache, we might try to repost the form.
            NavigationAction action;
            if (formData) {
                
                formData->generateFiles(m_frame->page()->chrome()->client());

                request.setHTTPMethod("POST");
                request.setHTTPBody(formData);
                request.setHTTPContentType(item->formContentType());
                RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::createFromString(item->referrer());
                addHTTPOriginIfNeeded(request, securityOrigin->toString());
        
                // Make sure to add extra fields to the request after the Origin header is added for the FormData case.
                // See https://bugs.webkit.org/show_bug.cgi?id=22194 for more discussion.
                addExtraFieldsToRequest(request, m_loadType, true, formData);
                addedExtraFields = true;
                
                // FIXME: Slight hack to test if the NSURL cache contains the page we're going to.
                // We want to know this before talking to the policy delegate, since it affects whether 
                // we show the DoYouReallyWantToRepost nag.
                //
                // This trick has a small bug (3123893) where we might find a cache hit, but then
                // have the item vanish when we try to use it in the ensuing nav.  This should be
                // extremely rare, but in that case the user will get an error on the navigation.
                
                if (ResourceHandle::willLoadFromCache(request))
                    action = NavigationAction(itemURL, loadType, false);
                else {
                    request.setCachePolicy(ReloadIgnoringCacheData);
                    action = NavigationAction(itemURL, NavigationTypeFormResubmitted);
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
                        if (itemURL.protocol() != "https")
                            request.setCachePolicy(ReturnCacheDataElseLoad);
                        break;
                    case FrameLoadTypeStandard:
                    case FrameLoadTypeRedirect:
                        break;
                    case FrameLoadTypeSame:
                    default:
                        ASSERT_NOT_REACHED();
                }

                action = NavigationAction(itemOriginalURL, loadType, false);
            }
            
            if (!addedExtraFields)
                addExtraFieldsToRequest(request, m_loadType, true, formData);

            loadWithNavigationAction(request, action, loadType, 0);
        }
    }
}

// Walk the frame tree and ensure that the URLs match the URLs in the item.
bool FrameLoader::urlsMatchItem(HistoryItem* item) const
{
    const KURL& currentURL = documentLoader()->url();
    if (!equalIgnoringRef(currentURL, item->url()))
        return false;

    const HistoryItemVector& childItems = item->children();

    unsigned size = childItems.size();
    for (unsigned i = 0; i < size; ++i) {
        Frame* childFrame = m_frame->tree()->child(childItems[i]->target());
        if (childFrame && !childFrame->loader()->urlsMatchItem(childItems[i].get()))
            return false;
    }

    return true;
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
void FrameLoader::goToItem(HistoryItem* targetItem, FrameLoadType type)
{
    ASSERT(!m_frame->tree()->parent());
    
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    Page* page = m_frame->page();
    if (!page)
        return;
    if (!m_client->shouldGoToHistoryItem(targetItem))
        return;

    // Set the BF cursor before commit, which lets the user quickly click back/forward again.
    // - plus, it only makes sense for the top level of the operation through the frametree,
    // as opposed to happening for some/one of the page commits that might happen soon
    BackForwardList* bfList = page->backForwardList();
    HistoryItem* currentItem = bfList->currentItem();    
    bfList->goToItem(targetItem);
    Settings* settings = m_frame->settings();
    page->setGlobalHistoryItem((!settings || settings->privateBrowsingEnabled()) ? 0 : targetItem);
    recursiveGoToItem(targetItem, currentItem, type);
}

// The general idea here is to traverse the frame tree and the item tree in parallel,
// tracking whether each frame already has the content the item requests.  If there is
// a match (by URL), we just restore scroll position and recurse.  Otherwise we must
// reload that frame, and all its kids.
void FrameLoader::recursiveGoToItem(HistoryItem* item, HistoryItem* fromItem, FrameLoadType type)
{
    ASSERT(item);
    ASSERT(fromItem);
    
    KURL itemURL = item->url();
    KURL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->url();
    
    // Always reload the target frame of the item we're going to.  This ensures that we will
    // do -some- load for the transition, which means a proper notification will be posted
    // to the app.
    // The exact URL has to match, including fragment.  We want to go through the _load
    // method, even if to do a within-page navigation.
    // The current frame tree and the frame tree snapshot in the item have to match.
    if (!item->isTargetItem() &&
        itemURL == currentURL &&
        ((m_frame->tree()->name().isEmpty() && item->target().isEmpty()) || m_frame->tree()->name() == item->target()) &&
        childFramesMatchItem(item))
    {
        // This content is good, so leave it alone and look for children that need reloading
        // Save form state (works from currentItem, since prevItem is nil)
        ASSERT(!m_previousHistoryItem);
        saveDocumentState();
        saveScrollPositionAndViewStateToItem(m_currentHistoryItem.get());

        if (FrameView* view = m_frame->view())
            view->setWasScrolledByUser(false);

        m_currentHistoryItem = item;
                
        // Restore form state (works from currentItem)
        restoreDocumentState();
        
        // Restore the scroll position (we choose to do this rather than going back to the anchor point)
        restoreScrollPositionAndViewState();
        
        const HistoryItemVector& childItems = item->children();
        
        int size = childItems.size();
        for (int i = 0; i < size; ++i) {
            String childName = childItems[i]->target();
            HistoryItem* fromChildItem = fromItem->childItemWithName(childName);
            ASSERT(fromChildItem || fromItem->isTargetItem());
            Frame* childFrame = m_frame->tree()->child(childName);
            ASSERT(childFrame);
            childFrame->loader()->recursiveGoToItem(childItems[i].get(), fromChildItem, type);
        }
    } else {
        loadItem(item, type);
    }
}

// helper method that determines whether the subframes described by the item's subitems
// match our own current frameset
bool FrameLoader::childFramesMatchItem(HistoryItem* item) const
{
    const HistoryItemVector& childItems = item->children();
    if (childItems.size() != m_frame->tree()->childCount())
        return false;
    
    unsigned size = childItems.size();
    for (unsigned i = 0; i < size; ++i)
        if (!m_frame->tree()->child(childItems[i]->target()))
            return false;
    
    // Found matches for all item targets
    return true;
}

// There are 3 things you might think of as "history", all of which are handled by these functions.
//
//     1) Back/forward: The m_currentHistoryItem is part of this mechanism.
//     2) Global history: Handled by the client.
//     3) Visited links: Handled by the PageGroup.

void FrameLoader::updateHistoryForStandardLoad()
{
    LOG(History, "WebCoreHistory: Updating History for Standard Load in frame %s", documentLoader()->url().string().ascii().data());

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = documentLoader()->urlForHistory();

    // If the navigation occured during load and this is a subframe, update the current
    // back/forward item rather than adding a new one and don't add the new URL to global
    // history at all. But do add it to visited links. <rdar://problem/5333496>
    bool frameNavigationDuringLoad = false;
    if (m_navigationDuringLoad) {
        HTMLFrameOwnerElement* owner = m_frame->ownerElement();
        frameNavigationDuringLoad = owner && !owner->createdByParser();
        m_navigationDuringLoad = false;
    }

    if (!frameNavigationDuringLoad && !documentLoader()->isClientRedirect()) {
        if (!historyURL.isEmpty()) {
            addBackForwardItemClippedAtTarget(true);
            if (!needPrivacy)
                m_client->updateGlobalHistory();
            if (Page* page = m_frame->page())
                page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
        }
    } else if (documentLoader()->unreachableURL().isEmpty() && m_currentHistoryItem) {
        m_currentHistoryItem->setURL(documentLoader()->url());
        m_currentHistoryItem->setFormInfoFromRequest(documentLoader()->request());
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);
    }
}

void FrameLoader::updateHistoryForClientRedirect()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for client redirect in frame %s", documentLoader()->title().utf8().data());
#endif

    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // webcore has closed the URL and saved away the form state.
    if (m_currentHistoryItem) {
        m_currentHistoryItem->clearDocumentState();
        m_currentHistoryItem->clearScrollPoint();
    }

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = documentLoader()->urlForHistory();

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);
    }
}

void FrameLoader::updateHistoryForBackForwardNavigation()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for back/forward navigation in frame %s", documentLoader()->title().utf8().data());
#endif

    // Must grab the current scroll position before disturbing it
    saveScrollPositionAndViewStateToItem(m_previousHistoryItem.get());
}

void FrameLoader::updateHistoryForReload()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for reload in frame %s", documentLoader()->title().utf8().data());
#endif

    if (m_currentHistoryItem) {
        pageCache()->remove(m_currentHistoryItem.get());
    
        if (loadType() == FrameLoadTypeReload || loadType() == FrameLoadTypeReloadFromOrigin)
            saveScrollPositionAndViewStateToItem(m_currentHistoryItem.get());
    
        // Sometimes loading a page again leads to a different result because of cookies. Bugzilla 4072
        if (documentLoader()->unreachableURL().isEmpty())
            m_currentHistoryItem->setURL(documentLoader()->requestURL());
    }
}

void FrameLoader::updateHistoryForRedirectWithLockedHistory()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for internal load in frame %s", documentLoader()->title().utf8().data());
#endif
    
    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = documentLoader()->urlForHistory();

    if (documentLoader()->isClientRedirect()) {
        if (!m_currentHistoryItem && !m_frame->tree()->parent()) {
            addBackForwardItemClippedAtTarget(true);
            if (!historyURL.isEmpty()) {
                if (!needPrivacy)
                    m_client->updateGlobalHistory();
                if (Page* page = m_frame->page())
                    page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
            }
        }
        if (m_currentHistoryItem) {
            m_currentHistoryItem->setURL(documentLoader()->url());
            m_currentHistoryItem->setFormInfoFromRequest(documentLoader()->request());
        }
    } else {
        Frame* parentFrame = m_frame->tree()->parent();
        if (parentFrame && parentFrame->loader()->m_currentHistoryItem)
            parentFrame->loader()->m_currentHistoryItem->addChildItem(createHistoryItem(true));
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);
    }
}

void FrameLoader::updateHistoryForCommit()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for commit in frame %s", documentLoader()->title().utf8().data());
#endif
    FrameLoadType type = loadType();
    if (isBackForwardLoadType(type) ||
        ((type == FrameLoadTypeReload || type == FrameLoadTypeReloadFromOrigin) && !provisionalDocumentLoader()->unreachableURL().isEmpty())) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        m_previousHistoryItem = m_currentHistoryItem;
        ASSERT(m_provisionalHistoryItem);
        m_currentHistoryItem = m_provisionalHistoryItem;
        m_provisionalHistoryItem = 0;
    }
}

void FrameLoader::updateHistoryForAnchorScroll()
{
    if (m_URL.isEmpty())
        return;

    Settings* settings = m_frame->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->group().addVisitedLink(m_URL);
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
void FrameLoader::saveDocumentAndScrollState()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame)) {
        frame->loader()->saveDocumentState();
        frame->loader()->saveScrollPositionAndViewStateToItem(frame->loader()->currentHistoryItem());
    }
}

// FIXME: These 6 setter/getters are here for a dwindling number of users in WebKit, WebFrame
// being the primary one.  After they're no longer needed there, they can be removed!
HistoryItem* FrameLoader::currentHistoryItem()
{
    return m_currentHistoryItem.get();
}

HistoryItem* FrameLoader::previousHistoryItem()
{
    return m_previousHistoryItem.get();
}

HistoryItem* FrameLoader::provisionalHistoryItem()
{
    return m_provisionalHistoryItem.get();
}

void FrameLoader::setCurrentHistoryItem(PassRefPtr<HistoryItem> item)
{
    m_currentHistoryItem = item;
}

void FrameLoader::setPreviousHistoryItem(PassRefPtr<HistoryItem> item)
{
    m_previousHistoryItem = item;
}

void FrameLoader::setProvisionalHistoryItem(PassRefPtr<HistoryItem> item)
{
    m_provisionalHistoryItem = item;
}

void FrameLoader::setMainDocumentError(DocumentLoader* loader, const ResourceError& error)
{
    m_client->setMainDocumentError(loader, error);
}

void FrameLoader::mainReceivedCompleteError(DocumentLoader* loader, const ResourceError&)
{
    loader->setPrimaryLoadComplete(true);
    m_client->dispatchDidLoadMainResource(activeDocumentLoader());
    checkCompleted();
    if (m_frame->page())
        checkLoadComplete();
}

void FrameLoader::mainReceivedError(const ResourceError& error, bool isComplete)
{
    activeDocumentLoader()->mainReceivedError(error, isComplete);
}

ResourceError FrameLoader::cancelledError(const ResourceRequest& request) const
{
    ResourceError error = m_client->cancelledError(request);
    error.setIsCancellation(true);
    return error;
}

ResourceError FrameLoader::blockedError(const ResourceRequest& request) const
{
    return m_client->blockedError(request);
}

ResourceError FrameLoader::cannotShowURLError(const ResourceRequest& request) const
{
    return m_client->cannotShowURLError(request);
}

ResourceError FrameLoader::fileDoesNotExistError(const ResourceResponse& response) const
{
    return m_client->fileDoesNotExistError(response);    
}

void FrameLoader::didFinishLoad(ResourceLoader* loader)
{    
    if (Page* page = m_frame->page())
        page->progress()->completeProgress(loader->identifier());
    dispatchDidFinishLoading(loader->documentLoader(), loader->identifier());
}

bool FrameLoader::shouldUseCredentialStorage(ResourceLoader* loader)
{
    return m_client->shouldUseCredentialStorage(loader->documentLoader(), loader->identifier());
}

void FrameLoader::didReceiveAuthenticationChallenge(ResourceLoader* loader, const AuthenticationChallenge& currentWebChallenge)
{
    m_client->dispatchDidReceiveAuthenticationChallenge(loader->documentLoader(), loader->identifier(), currentWebChallenge);
}

void FrameLoader::didCancelAuthenticationChallenge(ResourceLoader* loader, const AuthenticationChallenge& currentWebChallenge)
{
    m_client->dispatchDidCancelAuthenticationChallenge(loader->documentLoader(), loader->identifier(), currentWebChallenge);
}

PolicyCheck::PolicyCheck()
    : m_navigationFunction(0)
    , m_newWindowFunction(0)
    , m_contentFunction(0)
{
}

void PolicyCheck::clear()
{
    clearRequest();
    m_navigationFunction = 0;
    m_newWindowFunction = 0;
    m_contentFunction = 0;
}

void PolicyCheck::set(const ResourceRequest& request, PassRefPtr<FormState> formState,
    NavigationPolicyDecisionFunction function, void* argument)
{
    m_request = request;
    m_formState = formState;
    m_frameName = String();

    m_navigationFunction = function;
    m_newWindowFunction = 0;
    m_contentFunction = 0;
    m_argument = argument;
}

void PolicyCheck::set(const ResourceRequest& request, PassRefPtr<FormState> formState,
    const String& frameName, NewWindowPolicyDecisionFunction function, void* argument)
{
    m_request = request;
    m_formState = formState;
    m_frameName = frameName;

    m_navigationFunction = 0;
    m_newWindowFunction = function;
    m_contentFunction = 0;
    m_argument = argument;
}

void PolicyCheck::set(ContentPolicyDecisionFunction function, void* argument)
{
    m_request = ResourceRequest();
    m_formState = 0;
    m_frameName = String();

    m_navigationFunction = 0;
    m_newWindowFunction = 0;
    m_contentFunction = function;
    m_argument = argument;
}

void PolicyCheck::call(bool shouldContinue)
{
    if (m_navigationFunction)
        m_navigationFunction(m_argument, m_request, m_formState.get(), shouldContinue);
    if (m_newWindowFunction)
        m_newWindowFunction(m_argument, m_request, m_formState.get(), m_frameName, shouldContinue);
    ASSERT(!m_contentFunction);
}

void PolicyCheck::call(PolicyAction action)
{
    ASSERT(!m_navigationFunction);
    ASSERT(!m_newWindowFunction);
    ASSERT(m_contentFunction);
    m_contentFunction(m_argument, action);
}

void PolicyCheck::clearRequest()
{
    m_request = ResourceRequest();
    m_formState = 0;
    m_frameName = String();
}

void PolicyCheck::cancel()
{
    clearRequest();
    if (m_navigationFunction)
        m_navigationFunction(m_argument, m_request, m_formState.get(), false);
    if (m_newWindowFunction)
        m_newWindowFunction(m_argument, m_request, m_formState.get(), m_frameName, false);
    if (m_contentFunction)
        m_contentFunction(m_argument, PolicyIgnore);
}

void FrameLoader::setTitle(const String& title)
{
    documentLoader()->setTitle(title);
}

KURL FrameLoader::originalRequestURL() const
{
    return activeDocumentLoader()->originalRequest().url();
}

String FrameLoader::referrer() const
{
    return documentLoader()->request().httpReferrer();
}

void FrameLoader::dispatchWindowObjectAvailable()
{
    if (!m_frame->script()->isEnabled() || !m_frame->script()->haveWindowShell())
        return;

    m_client->windowObjectCleared();

    if (Page* page = m_frame->page()) {
        if (InspectorController* inspector = page->inspectorController())
            inspector->inspectedWindowScriptObjectCleared(m_frame);
        if (InspectorController* inspector = page->parentInspectorController())
            inspector->windowScriptObjectAvailable();
    }
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize& size, Element* element, const HashMap<String, String>& args)
{
    String baseURLString;
    Vector<String> paramNames;
    Vector<String> paramValues;
    HashMap<String, String>::const_iterator end = args.end();
    for (HashMap<String, String>::const_iterator it = args.begin(); it != end; ++it) {
        if (equalIgnoringCase(it->first, "baseurl"))
            baseURLString = it->second;
        paramNames.append(it->first);
        paramValues.append(it->second);
    }
    
    if (baseURLString.isEmpty())
        baseURLString = m_frame->document()->baseURL().string();
    KURL baseURL = completeURL(baseURLString);

    Widget* widget = m_client->createJavaAppletWidget(size, element, baseURL, paramNames, paramValues);
    if (widget)
        m_containsPlugIns = true;
    
    return widget;
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    m_client->didChangeTitle(loader);

    // The title doesn't get communicated to the WebView until we are committed.
    if (loader->isCommitted()) {
        // Must update the entries in the back-forward list too.
        if (m_currentHistoryItem)
            m_currentHistoryItem->setTitle(loader->title());
        // This must go through the WebFrame because it has the right notion of the current b/f item.
        m_client->setTitle(loader->title(), loader->urlForHistory());
        m_client->setMainFrameDocumentReady(true); // update observers with new DOMDocument
        m_client->dispatchDidReceiveTitle(loader->title());
    }
}

void FrameLoader::continueLoadWithData(SharedBuffer* buffer, const String& mimeType, const String& textEncoding, const KURL& url)
{
    m_responseMIMEType = mimeType;
    didOpenURL(url);

    String encoding;
    if (m_frame)
        encoding = documentLoader()->overrideEncoding();
    bool userChosen = !encoding.isNull();
    if (encoding.isNull())
        encoding = textEncoding;
    setEncoding(encoding, userChosen);

    ASSERT(m_frame->document());

    addData(buffer->data(), buffer->size());
}

void FrameLoader::registerURLSchemeAsLocal(const String& scheme)
{
    localSchemes().add(scheme);
}

bool FrameLoader::shouldTreatURLAsLocal(const String& url)
{
    // This avoids an allocation of another String and the HashSet contains()
    // call for the file: and http: schemes.
    if (url.length() >= 5) {
        const UChar* s = url.characters();
        if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p' && s[4] == ':')
            return false;
        if (s[0] == 'f' && s[1] == 'i' && s[2] == 'l' && s[3] == 'e' && s[4] == ':')
            return true;
    }

    int loc = url.find(':');
    if (loc == -1)
        return false;

    String scheme = url.left(loc);
    return localSchemes().contains(scheme);
}

bool FrameLoader::shouldTreatSchemeAsLocal(const String& scheme)
{
    // This avoids an allocation of another String and the HashSet contains()
    // call for the file: and http: schemes.
    if (scheme.length() == 4) {
        const UChar* s = scheme.characters();
        if (s[0] == 'h' && s[1] == 't' && s[2] == 't' && s[3] == 'p')
            return false;
        if (s[0] == 'f' && s[1] == 'i' && s[2] == 'l' && s[3] == 'e')
            return true;
    }

    if (scheme.isEmpty())
        return false;

    return localSchemes().contains(scheme);
}

void FrameLoader::dispatchDidCommitLoad()
{
    if (m_creatingInitialEmptyDocument)
        return;

#ifndef NDEBUG
    m_didDispatchDidCommitLoad = true;
#endif

    m_client->dispatchDidCommitLoad();

    if (Page* page = m_frame->page())
        page->inspectorController()->didCommitLoad(m_documentLoader.get());
}

void FrameLoader::dispatchAssignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    m_client->assignIdentifierToInitialRequest(identifier, loader, request);

    if (Page* page = m_frame->page())
        page->inspectorController()->identifierForInitialRequest(identifier, loader, request);
}

void FrameLoader::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    StringImpl* oldRequestURL = request.url().string().impl();
    m_documentLoader->didTellClientAboutLoad(request.url());

    m_client->dispatchWillSendRequest(loader, identifier, request, redirectResponse);

    // If the URL changed, then we want to put that new URL in the "did tell client" set too.
    if (!request.isNull() && oldRequestURL != request.url().string().impl())
        m_documentLoader->didTellClientAboutLoad(request.url());

    if (Page* page = m_frame->page())
        page->inspectorController()->willSendRequest(loader, identifier, request, redirectResponse);
}

void FrameLoader::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    m_client->dispatchDidReceiveResponse(loader, identifier, r);

    if (Page* page = m_frame->page())
        page->inspectorController()->didReceiveResponse(loader, identifier, r);
}

void FrameLoader::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    m_client->dispatchDidReceiveContentLength(loader, identifier, length);

    if (Page* page = m_frame->page())
        page->inspectorController()->didReceiveContentLength(loader, identifier, length);
}

void FrameLoader::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    m_client->dispatchDidFinishLoading(loader, identifier);

    if (Page* page = m_frame->page())
        page->inspectorController()->didFinishLoading(loader, identifier);
}

void FrameLoader::tellClientAboutPastMemoryCacheLoads()
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->page()->areMemoryCacheClientCallsEnabled());

    if (!m_documentLoader)
        return;

    Vector<String> pastLoads;
    m_documentLoader->takeMemoryCacheLoadsForClientNotification(pastLoads);

    size_t size = pastLoads.size();
    for (size_t i = 0; i < size; ++i) {
        CachedResource* resource = cache()->resourceForURL(pastLoads[i]);

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

#if USE(LOW_BANDWIDTH_DISPLAY)

bool FrameLoader::addLowBandwidthDisplayRequest(CachedResource* cache)
{
    if (m_frame->document()->inLowBandwidthDisplay() == false)
        return false;
        
    // if cache is loaded, don't add to the list, where notifyFinished() is expected. 
    if (cache->isLoaded())
        return false;
                
    switch (cache->type()) {
        case CachedResource::CSSStyleSheet:
        case CachedResource::Script:
            m_needToSwitchOutLowBandwidthDisplay = true;
            m_externalRequestsInLowBandwidthDisplay.add(cache);
            cache->addClient(this);
            return true;
        case CachedResource::ImageResource:
        case CachedResource::FontResource:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
#if ENABLE(XBL)
        case CachedResource::XBLStyleSheet:
#endif
            return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void FrameLoader::removeAllLowBandwidthDisplayRequests()
{
    HashSet<CachedResource*>::iterator end = m_externalRequestsInLowBandwidthDisplay.end();
    for (HashSet<CachedResource*>::iterator it = m_externalRequestsInLowBandwidthDisplay.begin(); it != end; ++it)
        (*it)->removeClient(this);
    m_externalRequestsInLowBandwidthDisplay.clear();
}
    
void FrameLoader::notifyFinished(CachedResource* script)
{
    HashSet<CachedResource*>::iterator it = m_externalRequestsInLowBandwidthDisplay.find(script);
    if (it != m_externalRequestsInLowBandwidthDisplay.end()) {
        (*it)->removeClient(this);
        m_externalRequestsInLowBandwidthDisplay.remove(it);
        switchOutLowBandwidthDisplayIfReady();        
    }
}

void FrameLoader::switchOutLowBandwidthDisplayIfReady()
{
    RefPtr<Document> oldDoc = m_frame->document();
    if (oldDoc->inLowBandwidthDisplay()) {
        if (!m_needToSwitchOutLowBandwidthDisplay) {
            // no need to switch, just reset state
            oldDoc->setLowBandwidthDisplay(false);
            removeAllLowBandwidthDisplayRequests();        
            m_pendingSourceInLowBandwidthDisplay = String();
            m_finishedParsingDuringLowBandwidthDisplay = false;
            return;
        } else if (m_externalRequestsInLowBandwidthDisplay.isEmpty() || 
            m_pendingSourceInLowBandwidthDisplay.length() > cMaxPendingSourceLengthInLowBandwidthDisplay) {
            // clear the flag first
            oldDoc->setLowBandwidthDisplay(false);
            
            // similar to clear(), should be refactored to share more code
            oldDoc->cancelParsing();
            oldDoc->detach();
            if (m_frame->script()->isEnabled())
                m_frame->script()->clearWindowShell();
            if (m_frame->view())
                m_frame->view()->clear();
            
            // similar to begin(), should be refactored to share more code
            RefPtr<Document> newDoc = DOMImplementation::createDocument(m_responseMIMEType, m_frame, m_frame->inViewSourceMode());
            m_frame->setDocument(newDoc);
            newDoc->setURL(m_URL);
            if (m_decoder)
                newDoc->setDecoder(m_decoder.get());
            restoreDocumentState();
            dispatchWindowObjectAvailable();         
            newDoc->implicitOpen();

            // swap DocLoader ownership
            DocLoader* docLoader = newDoc->docLoader();
            newDoc->setDocLoader(oldDoc->docLoader());
            newDoc->docLoader()->replaceDocument(newDoc.get());
            docLoader->replaceDocument(oldDoc.get());
            oldDoc->setDocLoader(docLoader);
            
            // drop the old doc            
            oldDoc = 0;

            // write decoded data to the new doc, similar to write()
            if (m_pendingSourceInLowBandwidthDisplay.length()) {
                if (m_decoder->encoding().usesVisualOrdering())
                    newDoc->setVisuallyOrdered();                    
                newDoc->recalcStyle(Node::Force);                
                newDoc->tokenizer()->write(m_pendingSourceInLowBandwidthDisplay, true);
            
                if (m_finishedParsingDuringLowBandwidthDisplay)
                    newDoc->finishParsing();
            }

            // update rendering
            newDoc->updateRendering();
                        
            // reset states
            removeAllLowBandwidthDisplayRequests();        
            m_pendingSourceInLowBandwidthDisplay = String();
            m_finishedParsingDuringLowBandwidthDisplay = false;
            m_needToSwitchOutLowBandwidthDisplay = false;
        }
    }
}

#endif

bool FrameLoaderClient::hasHTMLView() const
{
    return true;
}

} // namespace WebCore
