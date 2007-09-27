/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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

#include "CString.h"
#include "Cache.h"
#include "CachedPage.h"
#include "Chrome.h"
#include "DOMImplementation.h"
#include "DocLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
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
#include "FramePrivate.h"
#include "FrameTree.h"
#include "FrameView.h"
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
#include "MainResourceLoader.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PageCache.h"
#include "ProgressTracker.h"
#include "RenderPart.h"
#include "RenderWidget.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "SystemTime.h"
#include "TextResourceDecoder.h"
#include "WindowFeatures.h"
#include "XMLHttpRequest.h"
#include "XMLTokenizer.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include <kjs/JSLock.h>
#include <kjs/object.h>

using KJS::UString;
using KJS::JSLock;
using KJS::JSValue;

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

#if USE(LOW_BANDWIDTH_DISPLAY)
const unsigned int cMaxPendingSourceLengthInLowBandwidthDisplay = 128 * 1024;
#endif

struct FormSubmission {
    const char* action;
    String URL;
    RefPtr<FormData> data;
    String target;
    String contentType;
    String boundary;
    RefPtr<Event> event;

    FormSubmission(const char* a, const String& u, PassRefPtr<FormData> d, const String& t,
            const String& ct, const String& b, PassRefPtr<Event> e)
        : action(a)
        , URL(u)
        , data(d)
        , target(t)
        , contentType(ct)
        , boundary(b)
        , event(e)
    {
    }
};

struct ScheduledRedirection {
    enum Type { redirection, locationChange, historyNavigation, locationChangeDuringLoad };
    Type type;
    double delay;
    String URL;
    String referrer;
    int historySteps;
    bool lockHistory;
    bool wasUserGesture;

    ScheduledRedirection(double redirectDelay, const String& redirectURL, bool redirectLockHistory, bool userGesture)
        : type(redirection)
        , delay(redirectDelay)
        , URL(redirectURL)
        , historySteps(0)
        , lockHistory(redirectLockHistory)
        , wasUserGesture(userGesture)
    {
    }

    ScheduledRedirection(Type locationChangeType,
            const String& locationChangeURL, const String& locationChangeReferrer,
            bool locationChangeLockHistory, bool locationChangeWasUserGesture)
        : type(locationChangeType)
        , delay(0)
        , URL(locationChangeURL)
        , referrer(locationChangeReferrer)
        , historySteps(0)
        , lockHistory(locationChangeLockHistory)
        , wasUserGesture(locationChangeWasUserGesture)
    {
    }

    explicit ScheduledRedirection(int historyNavigationSteps)
        : type(historyNavigation)
        , delay(0)
        , historySteps(historyNavigationSteps)
        , lockHistory(false)
        , wasUserGesture(false)
    {
    }
};

static double storedTimeOfLastCompletedLoad;
static bool m_restrictAccessToLocal = false;

static bool getString(JSValue* result, String& string)
{
    if (!result)
        return false;
    JSLock lock;
    UString ustring;
    if (!result->getString(ustring))
        return false;
    string = ustring;
    return true;
}

bool isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
        case FrameLoadTypeStandard:
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadAllowingStaleData:
        case FrameLoadTypeSame:
        case FrameLoadTypeRedirectWithLockedHistory:
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
    , m_cachePolicy(CachePolicyVerify)
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
#if USE(LOW_BANDWIDTH_DISPLAY)
    , m_useLowBandwidthDisplay(true)
    , m_finishedParsingDuringLowBandwidthDisplay(false)
    , m_needToSwitchOutLowBandwidthDisplay(false)
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
    setPolicyDocumentLoader(m_client->createDocumentLoader(ResourceRequest(String("")), SubstituteData()).get());
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
    m_client->setDefersLoading(defers);
}

Frame* FrameLoader::createWindow(const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{ 
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank")
        if (Frame* frame = m_frame->tree()->find(request.frameName())) {
            if (!request.resourceRequest().url().isEmpty())
                frame->loader()->load(request, false, true, 0, 0, HashMap<String, String>());
            if (Page* page = frame->page())
                page->chrome()->focus();
            created = false;
            return frame;
        }

    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    requestWithReferrer.resourceRequest().setHTTPReferrer(m_outgoingReferrer);
    
    Page* page = m_frame->page();
    if (page) {
        if (features.dialog)
            page = page->chrome()->createModalDialog(m_frame, requestWithReferrer);
        else
            page = page->chrome()->createWindow(m_frame, requestWithReferrer);
    }
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

void FrameLoader::changeLocation(const String& URL, const String& referrer, bool lockHistory, bool userGesture)
{
    changeLocation(completeURL(URL), referrer, lockHistory, userGesture);
}

void FrameLoader::changeLocation(const KURL& URL, const String& referrer, bool lockHistory, bool userGesture)
{
    if (URL.url().find("javascript:", 0, false) == 0) {
        String script = KURL::decode_string(URL.url().mid(strlen("javascript:")));
        JSValue* result = executeScript(script, userGesture);
        String scriptResult;
        if (getString(result, scriptResult)) {
            begin(m_URL);
            write(scriptResult);
            end();
        }
        return;
    }

    ResourceRequestCachePolicy policy = (m_cachePolicy == CachePolicyReload) || (m_cachePolicy == CachePolicyRefresh)
        ? ReloadIgnoringCacheData : UseProtocolCachePolicy;
    ResourceRequest request(URL, referrer, policy);
    
    urlSelected(request, "_self", 0, lockHistory, userGesture);
}

void FrameLoader::urlSelected(const ResourceRequest& request, const String& _target, Event* triggeringEvent, bool lockHistory, bool userGesture)
{
    String target = _target;
    if (target.isEmpty() && m_frame->document())
        target = m_frame->document()->baseTarget();

    const KURL& url = request.url();
    if (url.url().startsWith("javascript:", false)) {
        executeScript(KURL::decode_string(url.url().mid(strlen("javascript:"))), true);
        return;
    }

    FrameLoadRequest frameRequest(request, target);

    if (frameRequest.resourceRequest().httpReferrer().isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    urlSelected(frameRequest, triggeringEvent, lockHistory, userGesture);
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
    if (urlString.startsWith("javascript:", false)) {
        scriptURL = urlString.deprecatedString();
        url = "about:blank";
    } else
        url = completeURL(urlString);

    Frame* frame = ownerElement->contentFrame();
    if (frame)
        frame->loader()->scheduleLocationChange(url.url(), m_outgoingReferrer, true, userGestureHint());
    else
        frame = loadSubframe(ownerElement, url, frameName, m_outgoingReferrer);
    
    if (!frame)
        return false;

    if (!scriptURL.isEmpty())
        frame->loader()->replaceContentsWithScriptResult(scriptURL);

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
        FrameLoader::reportLocalLoadFailed(m_frame->page(), url.url());
        return 0;
    }

    bool hideReferrer = shouldHideReferrer(url, referrer);
    Frame* frame = m_client->createFrame(url, name, ownerElement, hideReferrer ? String() : referrer,
                                         allowsScrolling, marginWidth, marginHeight);

    if (!frame)  {
        checkCallImplicitClose();
        return 0;
    }
    
    frame->loader()->m_isComplete = false;
    
    if (ownerElement->renderer() && frame->view())
        static_cast<RenderWidget*>(ownerElement->renderer())->setWidget(frame->view());
    
    checkCallImplicitClose();
    
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the 
    // completed() signal for the child by hand
    // FIXME: In this case the Frame will have finished loading before 
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (url.isEmpty() || url == "about:blank") {
        frame->loader()->completed();
        frame->loader()->checkCompleted();
    }

    return frame;
}

void FrameLoader::submitFormAgain()
{
    if (m_isRunningScript)
        return;
    OwnPtr<FormSubmission> form(m_deferredFormSubmission.release());
    if (form)
        submitForm(form->action, form->URL, form->data, form->target,
            form->contentType, form->boundary, form->event.get());
}

void FrameLoader::submitForm(const char* action, const String& url, PassRefPtr<FormData> formData,
    const String& target, const String& contentType, const String& boundary, Event* event)
{
    ASSERT(formData.get());
    
    KURL u = completeURL(url.isNull() ? "" : url);
    // FIXME: Do we really need to special-case an empty URL?
    // Would it be better to just go on with the form submisson and let the I/O fail?
    if (u.isEmpty())
        return;

    DeprecatedString urlString = u.url();
    if (urlString.startsWith("javascript:", false)) {
        m_isExecutingJavaScriptFormAction = true;
        executeScript(KURL::decode_string(urlString.mid(strlen("javascript:"))));
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    if (m_isRunningScript) {
        if (m_deferredFormSubmission)
            return;
        m_deferredFormSubmission.set(new FormSubmission(action, url, formData, target,
            contentType, boundary, event));
        return;
    }

    FrameLoadRequest frameRequest;

    if (!m_outgoingReferrer.isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    frameRequest.setFrameName(target.isEmpty() ? m_frame->document()->baseTarget() : target);

    // Handle mailto: forms
    bool mailtoForm = u.protocol() == "mailto";
    if (mailtoForm) {
        // Append body=
        String body;
        if (equalIgnoringCase(contentType, "multipart/form-data"))
            // FIXME: is this correct? I suspect not, but what site can we test this on?
            body = formData->flattenToString();
        else if (equalIgnoringCase(contentType, "text/plain"))
            // Convention seems to be to decode, and s/&/\n/
            body = KURL::decode_string(
                formData->flattenToString().replace('&', '\n')
                .replace('+', ' ').deprecatedString()); // Recode for the URL
        else
            body = formData->flattenToString();

        String query = u.query();
        if (!query.isEmpty())
            query.append('&');
        u.setQuery((query + "body=" + KURL::encode_string(body.deprecatedString())).deprecatedString());
    }

    if (strcmp(action, "GET") == 0) {
        if (!mailtoForm)
            u.setQuery(formData->flattenToString().deprecatedString());
    } else {
        frameRequest.resourceRequest().setHTTPBody(formData.get());
        frameRequest.resourceRequest().setHTTPMethod("POST");

        // construct some user headers if necessary
        if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
            frameRequest.resourceRequest().setHTTPContentType(contentType);
        else // contentType must be "multipart/form-data"
            frameRequest.resourceRequest().setHTTPContentType(contentType + "; boundary=" + boundary);
    }

    frameRequest.resourceRequest().setURL(u);

    submitForm(frameRequest, event);
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
                m_frame->document()->dispatchWindowEvent(unloadEvent, false, false);
                if (m_frame->document())
                    m_frame->document()->updateRendering();
                m_wasUnloadEventEmitted = true;
            }
        }
        if (m_frame->document() && !m_frame->document()->inPageCache())
            m_frame->document()->removeAllEventListenersFromAllNodes();
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing() (David)
    m_isLoadingMainResource = false;
    m_didCallImplicitClose = true; // don't want that one either
    m_cachePolicy = CachePolicyVerify; // Why here?

    if (m_frame->document() && m_frame->document()->parsing()) {
        finishedParsing();
        m_frame->document()->setParsing(false);
    }
  
    m_workingURL = KURL();

    if (Document* doc = m_frame->document()) {
        if (DocLoader* docLoader = doc->docLoader())
            cache()->loader()->cancelRequests(docLoader);
        XMLHttpRequest::cancelRequests(doc);
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
        return "";
        
    // If we have an iconURL from a Link element, return that
    if (m_frame->document() && !m_frame->document()->iconURL().isEmpty())
        return m_frame->document()->iconURL().deprecatedString();
        
    // Don't return a favicon iconURL unless we're http or https
    if (m_URL.protocol() != "http" && m_URL.protocol() != "https")
        return "";
        
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
    m_frame->editor()->setLastEditCommand(0);
    closeURL();

    m_isComplete = false;
    m_isLoadingMainResource = true;
    m_didCallImplicitClose = false;

    m_frame->setJSStatusBarText(String());
    m_frame->setJSDefaultStatusBarText(String());

    m_URL = url;
    if (m_URL.protocol().startsWith("http") && !m_URL.host().isEmpty() && m_URL.path().isEmpty())
        m_URL.setPath("/");
    m_workingURL = m_URL;

    started();

    return true;
}

void FrameLoader::didExplicitOpen()
{
    m_isComplete = false;
    m_didCallImplicitClose = false;

    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call. 
    // Cancelling redirection here works for all cases because document.open 
    // implicitly precedes document.write.
    cancelRedirection(); 
    if (m_frame->document()->URL() != "about:blank")
        m_URL = m_frame->document()->URL();
}

void FrameLoader::replaceContentsWithScriptResult(const KURL& url)
{
    JSValue* result = executeScript(KURL::decode_string(url.url().mid(strlen("javascript:"))));
    String scriptResult;
    if (!getString(result, scriptResult))
        return;
    begin();
    write(scriptResult);
    end();
}

JSValue* FrameLoader::executeScript(const String& script, bool forceUserGesture)
{
    return executeScript(forceUserGesture ? String() : String(m_URL.url()), 0, script);
}

JSValue* FrameLoader::executeScript(const String& URL, int baseLine, const String& script)
{
    KJSProxy* proxy = m_frame->scriptProxy();
    if (!proxy)
        return 0;

    bool wasRunningScript = m_isRunningScript;
    m_isRunningScript = true;

    JSValue* result = proxy->evaluate(URL, baseLine, script);

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
}

void FrameLoader::clear(bool clearWindowProperties, bool clearScriptObjects)
{
    // FIXME: Commenting out the below line causes <http://bugs.webkit.org/show_bug.cgi?id=11212>, but putting it
    // back causes a measurable performance regression which we will need to fix to restore the correct behavior
    // urlsBridgeKnowsAbout.clear();

    m_frame->editor()->clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;
    
    if (m_frame->document() && !m_frame->document()->inPageCache()) {
        m_frame->document()->cancelParsing();
        if (m_frame->document()->attached()) {
            m_frame->document()->willRemove();
            m_frame->document()->detach();
        }
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties) {
        m_frame->clearScriptProxy();
        m_frame->clearDOMWindow();
    }

    m_frame->selectionController()->clear();
    m_frame->eventHandler()->clear();
    if (m_frame->view())
        m_frame->view()->clear();

    // Do not drop the document before the script proxy and view are cleared, as some destructors
    // might still try to access the document.
    m_frame->setDocument(0);
    m_decoder = 0;

    m_containsPlugIns = false;
    
    if (clearScriptObjects)
        m_frame->clearScriptObjects();
  
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

    m_frame->document()->docLoader()->setCachePolicy(m_cachePolicy);
    m_workingURL = KURL();

    double delay;
    String URL;
    if (!m_documentLoader)
        return;
    if (!parseHTTPRefresh(m_documentLoader->response().httpHeaderField("Refresh"), false, delay, URL))
        return;

    if (URL.isEmpty())
        URL = m_URL.url();
    else
        URL = m_frame->document()->completeURL(URL);

    scheduleHTTPRedirection(delay, URL);
}

const String& FrameLoader::responseMIMEType() const
{
    return m_responseMIMEType;
}

void FrameLoader::setResponseMIMEType(const String& type)
{
    m_responseMIMEType = type;
}
    
bool FrameLoader::isSecureTransition(const KURL& fromURL, const KURL& toURL)
{ 
    // new window created by the application
    if (fromURL.isEmpty())
        return true;
    
    if (fromURL.isLocalFile())
        return true;
    
    if (equalIgnoringCase(fromURL.host(), toURL.host()) && equalIgnoringCase(fromURL.protocol(), toURL.protocol()) && fromURL.port() == toURL.port())
        return true;
    
    return false;
}

void FrameLoader::begin()
{
    begin(KURL());
}

void FrameLoader::begin(const KURL& url, bool dispatch)
{
    bool resetScripting = !(m_isDisplayingInitialEmptyDocument && m_frame->document() 
                            && isSecureTransition(m_frame->document()->securityPolicyURL(), url));
    clear(resetScripting, resetScripting);
    if (dispatch)
        dispatchWindowObjectAvailable();

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_isLoadingMainResource = true;
    m_isDisplayingInitialEmptyDocument = m_creatingInitialEmptyDocument;

    KURL ref(url);
    ref.setUser(DeprecatedString());
    ref.setPass(DeprecatedString());
    ref.setRef(DeprecatedString());
    m_outgoingReferrer = ref.url();
    m_URL = url;
    KURL baseurl;

    if (!m_URL.isEmpty())
        baseurl = m_URL;

    RefPtr<Document> document = DOMImplementation::instance()->createDocument(m_responseMIMEType, m_frame, m_frame->inViewSourceMode());
    m_frame->setDocument(document);

    document->setURL(m_URL.url());
    // We prefer m_baseURL over m_URL because m_URL changes when we are
    // about to load a new page.
    document->setBaseURL(baseurl.url());
    if (m_decoder)
        document->setDecoder(m_decoder.get());

    updatePolicyBaseURL();

    Settings* settings = document->settings();
    document->docLoader()->setAutoLoadImages(settings && settings->loadsImagesAutomatically());
    KURL userStyleSheet = settings ? settings->userStyleSheetLocation() : KURL();
    if (!userStyleSheet.isEmpty())
        m_frame->setUserStyleSheetLocation(userStyleSheet);

    restoreDocumentState();

    document->implicitOpen();

    if (m_frame->view())
        m_frame->view()->resizeContents(0, 0);

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
        m_decoder = new TextResourceDecoder(m_responseMIMEType, settings ? settings->defaultTextEncodingName() : String());
        if (!m_encoding.isNull())
            m_decoder->setEncoding(m_encoding,
                m_encodingWasChosenByUser ? TextResourceDecoder::UserChosenEncoding : TextResourceDecoder::EncodingFromHTTPHeader);
        if (m_frame->document())
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
    else // reset policy which is changed in switchOutLowBandwidthDisplayIfReady()
        m_frame->document()->docLoader()->setCachePolicy(m_cachePolicy);    
#endif

    if (!m_receivedData) {
        m_receivedData = true;
        m_frame->document()->determineParseMode(decoded);
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
    if (m_isLoadingMainResource)
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

    if (m_documentLoader && !m_documentLoader->isLoadingFromCachedPage())
        startIconLoader();
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
    String urlString(url.url());
    if (urlString.isEmpty())
        return;

    // If we're not reloading and the icon database doesn't say to load now then bail before we actually start the load
    if (loadType() != FrameLoadTypeReload) {
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
                iconDatabase()->iconForPageURL(m_URL.url(), IntSize(0, 0));
                iconDatabase()->iconForPageURL(originalRequestURL().url(), IntSize(0, 0));
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

bool FrameLoader::restrictAccessToLocal()
{
    return m_restrictAccessToLocal;
}

void FrameLoader::setRestrictAccessToLocal(bool access)
{
    m_restrictAccessToLocal = access;
}

static HashSet<String, CaseInsensitiveHash<String> >& localSchemes()
{
    static HashSet<String, CaseInsensitiveHash<String> > localSchemes;

    if (localSchemes.isEmpty()) {
        localSchemes.add("file");
        localSchemes.add("applewebdata");
    }

    return localSchemes;
}

void FrameLoader::commitIconURLToIconDatabase(const KURL& icon)
{
    ASSERT(iconDatabase());
    LOG(IconDatabase, "Committing iconURL %s to database for pageURLs %s and %s", icon.url().ascii(), m_URL.url().ascii(), originalRequestURL().url().ascii());
    iconDatabase()->setIconURLForPageURL(icon.url(), m_URL.url());
    iconDatabase()->setIconURLForPageURL(icon.url(), originalRequestURL().url());
}

void FrameLoader::restoreDocumentState()
{
    Document* doc = m_frame->document();
    if (!doc)
        return;
        
    HistoryItem* itemToRestore = 0;
    
    switch (loadType()) {
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadAllowingStaleData:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            break;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
        case FrameLoadTypeRedirectWithLockedHistory:
        case FrameLoadTypeStandard:
            itemToRestore = m_currentHistoryItem.get(); 
    }
    
    if (!itemToRestore)
        return;
        
    doc->setStateForNewFormElements(itemToRestore->documentState());
}

void FrameLoader::gotoAnchor()
{
    // If our URL has no ref, then we have no place we need to jump to.
    // OTOH if css target was set previously, we want to set it to 0, recalc
    // and possibly repaint because :target pseudo class may have been
    // set(See bug 11321)
    if (!m_URL.hasRef() &&
        !(m_frame->document() && m_frame->document()->getCSSTarget()))
        return;

    DeprecatedString ref = m_URL.encodedHtmlRef();
    if (!gotoAnchor(ref)) {
        // Can't use htmlRef() here because it doesn't know which encoding to use to decode.
        // Decoding here has to match encoding in completeURL, which means it has to use the
        // page's encoding rather than UTF-8.
        if (m_decoder)
            gotoAnchor(KURL::decode_string(ref, m_decoder->encoding()));
    }
}

void FrameLoader::finishedParsing()
{
    if (m_creatingInitialEmptyDocument)
        return;

    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    RefPtr<Frame> protector = m_frame->refCount() > 0 ? m_frame : 0;

    checkCompleted();

    if (!m_frame->view())
        return; // We are being destroyed by something checkCompleted called.

    // Check if the scrollbars are really needed for the content.
    // If not, remove them, relayout, and repaint.
    m_frame->view()->restoreScrollbar();

    m_client->dispatchDidFinishDocumentLoad();

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
    if (m_frame->page())
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

    // All frames completed -> set their domain to the frameset's domain
    // This must only be done when loading the frameset initially (#22039),
    // not when following a link in a frame (#44162).
    if (m_frame->document()) {
        String domain = m_frame->document()->domain();
        for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
            if (child->document())
                child->document()->setDomainInternal(domain);
    }

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
    return m_frame->document()->completeURL(url).deprecatedString();
}

void FrameLoader::scheduleHTTPRedirection(double delay, const String& url)
{
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
        
    // We want a new history item if the refresh timeout is > 1 second.
    if (!m_scheduledRedirection || delay <= m_scheduledRedirection->delay)
        scheduleRedirection(new ScheduledRedirection(delay, url, delay <= 1, false));
}

void FrameLoader::scheduleLocationChange(const String& url, const String& referrer, bool lockHistory, bool wasUserGesture)
{    
    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change.
    KURL u(url.deprecatedString());
    if (u.hasRef() && equalIgnoringRef(m_URL, u)) {
        changeLocation(url, referrer, lockHistory, wasUserGesture);
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
    scheduleRedirection(new ScheduledRedirection(type, url, referrer, lockHistory, wasUserGesture));
}

void FrameLoader::scheduleRefresh(bool wasUserGesture)
{
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
    scheduleRedirection(new ScheduledRedirection(type, m_URL.url(), m_outgoingReferrer, true, wasUserGesture));
    m_cachePolicy = CachePolicyRefresh;
}

bool FrameLoader::isScheduledLocationChangePending() const
{
    if (!m_scheduledRedirection)
        return false;
    switch (m_scheduledRedirection->type) {
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
    // navigation will always be allowed in the 0 steps case, which is OK because
    // that's supposed to force a reload.
    if (!canGoBackOrForward(steps)) {
        cancelRedirection();
        return;
    }

    // If the steps to navigate is not zero (which needs to force a reload), and if the URL we're going to navigate 
    // to is the same as the current one, except for the fragment part, we don't need to schedule the navigation.
    if (steps != 0 && equalIgnoringRef(m_URL, historyURL(steps))) {
        goBackOrForward(steps);
        return;
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
            int backListCount = list->forwardListCount();
            if (backListCount > 0)
                item = list->itemAtIndex(-backListCount);
        }
    }
    if (item)
        page->goToItem(item, FrameLoadTypeIndexedBackForward);
}

void FrameLoader::redirectionTimerFired(Timer<FrameLoader>*)
{
    OwnPtr<ScheduledRedirection> redirection(m_scheduledRedirection.release());

    switch (redirection->type) {
        case ScheduledRedirection::redirection:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::locationChangeDuringLoad:
            changeLocation(redirection->URL, redirection->referrer,
                redirection->lockHistory, redirection->wasUserGesture);
            return;
        case ScheduledRedirection::historyNavigation:
            if (redirection->historySteps == 0) {
                // Special case for go(0) from a frame -> reload only the frame
                urlSelected(m_URL, "", 0, redirection->lockHistory, redirection->wasUserGesture);
                return;
            }
            // go(i!=0) from a frame navigates into the history of the frame only,
            // in both IE and NS (but not in Mozilla). We can't easily do that.
            goBackOrForward(redirection->historySteps);
            return;
    }
    ASSERT_NOT_REACHED();
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

    Node* anchorNode = m_frame->document()->getElementById(AtomicString(name));
    if (!anchorNode)
        anchorNode = m_frame->document()->anchors()->namedItem(name, !m_frame->document()->inCompatMode());

    m_frame->document()->setCSSTarget(anchorNode); // Setting to null will clear the current target.
  
    // Implement the rule that "" and "top" both mean top of page as in other browsers.
    if (!anchorNode && !(name.isEmpty() || equalIgnoringCase(name, "top")))
        return false;

    // We need to update the layout before scrolling, otherwise we could
    // really mess things up if an anchor scroll comes at a bad moment.
    if (m_frame->document()) {
        m_frame->document()->updateRendering();
        // Only do a layout if changes have occurred that make it necessary.      
        if (m_frame->view() && m_frame->document()->renderer() && m_frame->document()->renderer()->needsLayout())
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
        renderer->enclosingLayer()->scrollRectToVisible(rect, RenderLayer::gAlignToEdgeIfNeeded, RenderLayer::gAlignTopAlways);

    return true;
}

bool FrameLoader::requestObject(RenderPart* renderer, const String& url, const AtomicString& frameName,
    const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return true;

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

    AtomicString uniqueFrameName = m_frame->tree()->uniqueChildName(frameName);
    element->setFrameName(uniqueFrameName);
    
    // FIXME: OK to always make a new frame? When does the old frame get removed?
    return loadSubframe(element, completedURL, uniqueFrameName, m_outgoingReferrer);
}

bool FrameLoader::shouldUsePlugin(const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
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

        if (!canLoad(url, frame()->document())) {
            FrameLoader::reportLocalLoadFailed(m_frame->page(), url.url());
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

void FrameLoader::recordFormValue(const String& name, const String& value, PassRefPtr<HTMLFormElement> element)
{
    m_formAboutToBeSubmitted = element;
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

    if (m_frame->document())
        m_frame->document()->initSecurityPolicyURL();
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

    if (canCachePage() && m_client->canCachePage() && !m_currentHistoryItem->isInPageCache())
        cachePageForHistoryItem(m_currentHistoryItem.get());
}

bool FrameLoader::userGestureHint()
{
    Frame* rootFrame = m_frame;
    while (rootFrame->tree()->parent())
        rootFrame = rootFrame->tree()->parent();

    if (rootFrame->scriptProxy())
        return rootFrame->scriptProxy()->interpreter()->wasRunByUserGesture();

    return true; // If JavaScript is disabled, a user gesture must have initiated the navigation
}

void FrameLoader::didNotOpenURL(const KURL& URL)
{
    if (m_submittedFormURL == URL)
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

bool FrameLoader::canCachePage()
{    
    // Cache the page, if possible.
    // Don't write to the cache if in the middle of a redirect, since we will want to
    // store the final page we end up on.
    // No point writing to the cache on a reload or loadSame, since we will just write
    // over it again when we leave that page.
    // FIXME: <rdar://problem/4886592> - We should work out the complexities of caching pages with frames as they
    // are the most interesting pages on the web, and often those that would benefit the most from caching!
    FrameLoadType loadType = this->loadType();

    return m_documentLoader
        && m_documentLoader->mainDocumentError().isNull()
        && !m_frame->tree()->childCount()
        && !m_frame->tree()->parent()
        // FIXME: If we ever change this so that pages with plug-ins will be cached,
        // we need to make sure that we don't cache pages that have outstanding NPObjects
        // (objects created by the plug-in). Since there is no way to pause/resume a Netscape plug-in,
        // they would need to be destroyed and then recreated, and there is no way that we can recreate
        // the right NPObjects. See <rdar://problem/5197041> for more information.
        && !m_containsPlugIns
        && !m_URL.protocol().startsWith("https")
        && m_frame->document()
        && !m_frame->document()->applets()->length()
        && !m_frame->document()->hasWindowEventListener(unloadEvent)
        && m_frame->page()
        && m_frame->page()->backForwardList()->enabled()
        && m_frame->page()->backForwardList()->capacity() > 0
        && m_frame->page()->settings()->usesPageCache()
        && m_currentHistoryItem
        && !isQuickRedirectComing()
        && loadType != FrameLoadTypeReload 
        && loadType != FrameLoadTypeReloadAllowingStaleData
        && loadType != FrameLoadTypeSame
        && !m_documentLoader->isLoadingInAPISense()
        && !m_documentLoader->isStopping();
}

void FrameLoader::updatePolicyBaseURL()
{
    if (m_frame->tree()->parent() && m_frame->tree()->parent()->document())
        setPolicyBaseURL(m_frame->tree()->parent()->document()->policyBaseURL());
    else
        setPolicyBaseURL(m_URL.url());
}

void FrameLoader::setPolicyBaseURL(const String& s)
{
    if (m_frame->document())
        m_frame->document()->setPolicyBaseURL(s);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->setPolicyBaseURL(s);
}

// This does the same kind of work that FrameLoader::openURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::scrollToAnchor(const KURL& URL)
{
    m_URL = URL;
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
    stopRedirectionTimer();
    m_scheduledRedirection.set(redirection);
    if (m_isComplete)
        startRedirectionTimer();
}

void FrameLoader::startRedirectionTimer()
{
    ASSERT(m_scheduledRedirection);

    m_redirectionTimer.stop();
    m_redirectionTimer.startOneShot(m_scheduledRedirection->delay);

    switch (m_scheduledRedirection->type) {
        case ScheduledRedirection::redirection:
        case ScheduledRedirection::locationChange:
        case ScheduledRedirection::locationChangeDuringLoad:
            clientRedirected(m_scheduledRedirection->URL.deprecatedString(),
                m_scheduledRedirection->delay,
                currentTime() + m_redirectionTimer.nextFireInterval(),
                m_scheduledRedirection->lockHistory,
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

void FrameLoader::finalSetupForReplace(DocumentLoader* loader)
{
    m_client->clearUnarchivingState(loader);
}

void FrameLoader::load(const KURL& URL, Event* event)
{
    load(ResourceRequest(URL), false, true, event, 0, HashMap<String, String>());
}

void FrameLoader::load(const FrameLoadRequest& request, bool lockHistory, bool userGesture, Event* event,
    HTMLFormElement* submitForm, const HashMap<String, String>& formValues)
{
    KURL url = request.resourceRequest().url();
 
    String referrer;
    String argsReferrer = request.resourceRequest().httpReferrer();
    if (!argsReferrer.isEmpty())
        referrer = argsReferrer;
    else
        referrer = m_outgoingReferrer;
 
    ASSERT(frame()->document());
    if (url.url().startsWith("file:", false)) {
        if (!canLoad(url, frame()->document()) && !canLoad(url, referrer)) {
            FrameLoader::reportLocalLoadFailed(m_frame->page(), url.url());
            return;
        }
    }

    if (shouldHideReferrer(url, referrer))
        referrer = String();
    
    Frame* targetFrame = m_frame->tree()->find(request.frameName());
    if (!canTarget(targetFrame))
        return;
        
    if (request.resourceRequest().httpMethod() != "POST") {
        FrameLoadType loadType;
        if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
            loadType = FrameLoadTypeReload;
        else if (lockHistory)
            loadType = FrameLoadTypeRedirectWithLockedHistory;
        else
            loadType = FrameLoadTypeStandard;    
    
        RefPtr<FormState> formState;
        if (submitForm && !formValues.isEmpty())
            formState = FormState::create(submitForm, formValues, m_frame);
        
        load(request.resourceRequest().url(), referrer, loadType, 
            request.frameName(), event, formState.release());
    } else
        post(request.resourceRequest().url(), referrer, request.frameName(), 
            request.resourceRequest().httpBody(), request.resourceRequest().httpContentType(), event, submitForm, formValues);

    if (targetFrame && targetFrame != m_frame)
        if (Page* page = targetFrame->page())
            page->chrome()->focus();
}

void FrameLoader::load(const KURL& URL, const String& referrer, FrameLoadType newLoadType,
    const String& frameName, Event* event, PassRefPtr<FormState> formState)
{
    bool isFormSubmission = formState;
    
    ResourceRequest request(URL);
    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    addExtraFieldsToRequest(request, true, event || isFormSubmission);
    if (newLoadType == FrameLoadTypeReload)
        request.setCachePolicy(ReloadIgnoringCacheData);

    ASSERT(newLoadType != FrameLoadTypeSame);

    NavigationAction action(URL, newLoadType, isFormSubmission, event);

    if (!frameName.isEmpty()) {
        if (Frame* targetFrame = m_frame->tree()->find(frameName))
            targetFrame->loader()->load(URL, referrer, newLoadType, String(), event, formState);
        else
            checkNewWindowPolicy(action, request, formState, frameName);
        return;
    }

    RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;

    bool sameURL = shouldTreatURLAsSameAsCurrent(URL);
    
    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (!isFormSubmission
        && newLoadType != FrameLoadTypeReload
        && newLoadType != FrameLoadTypeSame
        && !shouldReload(URL, url())
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && !m_frame->isFrameSet()) {

        // Just do anchor navigation within the existing content.
        
        // We don't do this if we are submitting a form, explicitly reloading,
        // currently displaying a frameset, or if the new URL does not have a fragment.
        // These rules are based on what KHTML was doing in KHTMLPart::openURL.
        
        // FIXME: What about load types other than Standard and Reload?
        
        oldDocumentLoader->setTriggeringAction(action);
        stopPolicyCheck();
        checkNavigationPolicy(request, oldDocumentLoader.get(), formState,
            callContinueFragmentScrollAfterNavigationPolicy, this);
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        bool isRedirect = m_quickRedirectComing;
        load(request, action, newLoadType, formState);
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

    Frame* frame = m_frame->tree()->find(frameName);
    if (frame) {
        frame->loader()->load(request);
        return;
    }

    checkNewWindowPolicy(NavigationAction(request.url(), NavigationTypeOther), request, 0, frameName);
}

void FrameLoader::load(const ResourceRequest& request, const NavigationAction& action, FrameLoadType type, PassRefPtr<FormState> formState)
{
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, SubstituteData());

    loader->setTriggeringAction(action);
    if (m_documentLoader)
        loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    load(loader.get(), type, formState);
}

void FrameLoader::load(DocumentLoader* newDocumentLoader)
{
    ResourceRequest& r = newDocumentLoader->request();
    addExtraFieldsToRequest(r, true, false);
    FrameLoadType type;

    if (shouldTreatURLAsSameAsCurrent(newDocumentLoader->originalRequest().url())) {
        r.setCachePolicy(ReloadIgnoringCacheData);
        type = FrameLoadTypeSame;
    } else
        type = FrameLoadTypeStandard;

    if (m_documentLoader)
        newDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if (shouldReloadToHandleUnreachableURL(newDocumentLoader)) {
        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }

    load(newDocumentLoader, type, 0);
}

void FrameLoader::load(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> formState)
{
    ASSERT(m_client->hasWebView());

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT(m_client->hasFrameView());

    m_policyLoadType = type;

    if (Frame* parent = m_frame->tree()->parent())
        loader->setOverrideEncoding(parent->loader()->documentLoader()->overrideEncoding());

    stopPolicyCheck();
    setPolicyDocumentLoader(loader);

    checkNavigationPolicy(loader->request(), loader, formState,
        callContinueLoadAfterNavigationPolicy, this);
}

// FIXME: It would be nice if we could collapse these into one or two functions.
bool FrameLoader::canLoad(const KURL& url, const String& referrer)
{
    if (!shouldTreatURLAsLocal(url.url()))
        return true;

    return shouldTreatURLAsLocal(referrer);
}

bool FrameLoader::canLoad(const KURL& url, const Document* doc)
{
    if (!shouldTreatURLAsLocal(url.url()))
        return true;

    return doc && doc->isAllowedToLoadLocalResources();
}

bool FrameLoader::canLoad(const CachedResource& resource, const Document* doc)
{
    if (!resource.treatAsLocal())
        return true;

    return doc && doc->isAllowedToLoadLocalResources();
}

void FrameLoader::reportLocalLoadFailed(const Page* page, const String& url)
{
    ASSERT(!url.isEmpty());
    if (page)
        page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, "Not allowed to load local resource: " + url, 0, String());
}

bool FrameLoader::shouldHideReferrer(const KURL& url, const String& referrer)
{
    bool referrerIsSecureURL = referrer.startsWith("https:", false);
    bool referrerIsWebURL = referrerIsSecureURL || referrer.startsWith("http:", false);

    if (!referrerIsWebURL)
        return true;

    if (!referrerIsSecureURL)
        return false;

    bool URLIsSecureURL = url.url().startsWith("https:", false);

    return !URLIsSecureURL;
}

const ResourceRequest& FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->initialRequest();
}

void FrameLoader::receivedData(const char* data, int length)
{
    activeDocumentLoader()->receivedData(data, length);
}

bool FrameLoader::willUseArchive(ResourceLoader* loader, const ResourceRequest& request, const KURL& originalURL) const
{
    return m_client->willUseArchive(loader, request, originalURL);
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

void FrameLoader::reloadAllowingStaleData(const String& encoding)
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

    load(loader.get(), FrameLoadTypeReloadAllowingStaleData, 0);
}

void FrameLoader::reload()
{
    if (!m_documentLoader)
        return;

    ResourceRequest& initialRequest = m_documentLoader->request();
    
    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if (initialRequest.url().isEmpty())
        return;

    // Replace error-page URL with the URL we were trying to reach.
    KURL unreachableURL = m_documentLoader->unreachableURL();
    if (!unreachableURL.isEmpty())
        initialRequest = ResourceRequest(unreachableURL);
    
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(initialRequest, SubstituteData());

    ResourceRequest& request = loader->request();

    request.setCachePolicy(ReloadIgnoringCacheData);
    request.setHTTPHeaderField("Cache-Control", "max-age=0");

    // If we're about to re-post, set up action so the application can warn the user.
    if (request.httpMethod() == "POST")
        loader->setTriggeringAction(NavigationAction(request.url(), NavigationTypeFormResubmitted));

    loader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    load(loader.get(), FrameLoadTypeReload, 0);
}

bool FrameLoader::canTarget(Frame* target) const
{
    // This function prevents this exploit:
    // <rdar://problem/3715785> multiple frame injection vulnerability reported by Secunia, affects almost all browsers

    // Allow if there is no specific target.
    if (!target)
        return true;

    // Allow navigation within the same page/frameset.
    if (m_frame->page() == target->page())
        return true;

    // Allow if the request is made from a local file.
    ASSERT(m_frame->document());
    String domain = m_frame->document()->domain();
    if (domain.isEmpty())
        return true;
    
    // Allow if target is an entire window (top level frame of a window).
    Frame* parent = target->tree()->parent();
    if (!parent)
        return true;
    
    // Allow if the domain of the parent of the targeted frame equals this domain.
    String parentDomain;
    if (Document* parentDocument = parent->document())
        parentDomain = parentDocument->domain();
    return equalIgnoringCase(parentDomain, domain);
}

void FrameLoader::stopLoadingSubframes()
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
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
    m_client->clearArchivedResources();

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

void FrameLoader::cancelPendingArchiveLoad(ResourceLoader* loader)
{
    m_client->cancelPendingArchiveLoad(loader);
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
   
DocumentLoader* FrameLoader::provisionalDocumentLoader()
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
            url = pdl->URL();
        if (url.isEmpty())
            url = pdl->responseURL();
        if (url.isEmpty())
            url = "about:blank";

        didOpenURL(url);
    }
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
                    if (cachedPage)
                        m_client->setDocumentViewFromCachedPage(cachedPage.get());
                    else
                        m_client->makeDocumentView();
                }
            break;

        case FrameLoadTypeReload:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            updateHistoryForReload();
            m_client->makeDocumentView();
            break;

        // FIXME - just get rid of this case, and merge FrameLoadTypeReloadAllowingStaleData with the above case
        case FrameLoadTypeReloadAllowingStaleData:
            m_client->makeDocumentView();
            break;

        case FrameLoadTypeStandard:
            updateHistoryForStandardLoad();
            if (m_frame->view())
                m_frame->view()->suppressScrollbars(true);
            m_client->makeDocumentView();
            break;

        case FrameLoadTypeRedirectWithLockedHistory:
            updateHistoryForRedirectWithLockedHistory();
            m_client->makeDocumentView();
            break;

        // FIXME Remove this check when dummy ds is removed (whatever "dummy ds" is).
        // An exception should be thrown if we're in the FrameLoadTypeUninitialized state.
        default:
            ASSERT_NOT_REACHED();
    }

    m_responseMIMEType = dl->responseMIMEType();

    // Tell the client we've committed this URL.
    ASSERT(m_client->hasFrameView());

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

bool FrameLoader::privateBrowsingEnabled() const
{
    return m_client->privateBrowsingEnabled();
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

void FrameLoader::clientRedirected(const KURL& URL, double seconds, double fireDate, bool lockHistory, bool isJavaScriptFormAction)
{
    m_client->dispatchWillPerformClientRedirect(URL, seconds, fireDate);
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    m_sentRedirectNotification = true;
    
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    m_quickRedirectComing = lockHistory && m_documentLoader && !isJavaScriptFormAction;
}

bool FrameLoader::shouldReload(const KURL& currentURL, const KURL& destinationURL)
{
    // This function implements the rule: "Don't reload if navigating by fragment within
    // the same URL, but do reload if going to a new URL or to the same URL with no
    // fragment identifier at all."
    if (!currentURL.hasRef() && !destinationURL.hasRef())
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
    Settings* settings = m_frame->settings();
    if (settings && settings->isJavaScriptEnabled()) {
        m_frame->setJSStatusBarText(String());
        m_frame->setJSDefaultStatusBarText(String());
    }

    KURL URL = cachedPage.URL();

    if (URL.protocol().startsWith("http") && !URL.host().isEmpty() && URL.path().isEmpty())
        URL.setPath("/");
    
    m_URL = URL;
    m_workingURL = URL;

    started();

    clear();

    Document* document = cachedPage.document();
    ASSERT(document);
    document->setInPageCache(false);

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_outgoingReferrer = URL.url();

    FrameView* view = cachedPage.view();
    if (view)
        view->setWasScrolledByUser(false);
    m_frame->setView(view);
    
    m_frame->setDocument(document);
    m_decoder = document->decoder();

    updatePolicyBaseURL();

    cachedPage.restore(m_frame->page());

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

// FIXME: Which one of these URL methods is right?

KURL FrameLoader::url() const
{
    return m_URL;
}

KURL FrameLoader::URL() const
{
    return activeDocumentLoader()->URL();
}

bool FrameLoader::isArchiveLoadPending(ResourceLoader* loader) const
{
    return m_client->isArchiveLoadPending(loader);
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
#if PLATFORM(WIN)
    if (!m_creatingInitialEmptyDocument)
#endif
        m_client->finishedLoading(loader);
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
                if (Page* page = m_frame->page())
                    page->backForwardList()->goToItem(item.get());
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
                if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload) && page->backForwardList())
                    restoreScrollPositionAndViewState();

            if (m_creatingInitialEmptyDocument || !m_committedFirstRealDocumentLoad)
                return;

            const ResourceError& error = dl->mainDocumentError();
            if (!error.isNull())
                m_client->dispatchDidFailLoad(error);
            else
                m_client->dispatchDidFinishLoad();

            if (Page* page = m_frame->page())
                page->progress()->progressCompleted(m_frame);
            return;
        }
        
        case FrameStateComplete:
            // Even if already complete, we might have set a previous item on a frame that
            // didn't do any data loading on the past transaction. Make sure to clear these out.
            m_client->frameLoadCompleted();
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

    m_provisionalDocumentLoader->prepareForLoadStart();

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

void FrameLoader::frameLoadCompleted()
{
    m_client->frameLoadCompleted();

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

FrameLoaderClient* FrameLoader::client() const
{
    return m_client;
}

void FrameLoader::submitForm(const FrameLoadRequest& request, Event* event)
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

    // FIXME: We should probably call userGestureHint() to tell whether this form submission was the result of a user gesture.
    load(request, false, true, event, m_formAboutToBeSubmitted.get(), m_formValuesAboutToBeSubmitted);

    clearRecordedFormValues();
}

void FrameLoader::urlSelected(const FrameLoadRequest& request, Event* event, bool lockHistory, bool userGesture)
{
    FrameLoadRequest copy = request;
    if (copy.resourceRequest().httpReferrer().isEmpty())
        copy.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    load(copy, lockHistory, userGesture, event, 0, HashMap<String, String>());
}
    
String FrameLoader::userAgent(const KURL& url) const
{
    return m_client->userAgent(url);
}

void FrameLoader::tokenizerProcessedData()
{
    ASSERT(m_frame->page());
    ASSERT(m_frame->document());

    checkCompleted();
}

void FrameLoader::didTellBridgeAboutLoad(const String& URL)
{
    m_urlsBridgeKnowsAbout.add(URL);
}

bool FrameLoader::haveToldBridgeAboutLoad(const String& URL)
{
    return m_urlsBridgeKnowsAbout.contains(URL);
}

void FrameLoader::handledOnloadEvents()
{
    m_client->dispatchDidHandleOnloadEvents();
}

void FrameLoader::frameDetached()
{
    stopAllLoaders();
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
#if PLATFORM(MAC)
    [m_frame->bridge() close];
#endif
    m_client->detachedFromParent4();
}

void FrameLoader::dispatchDidChangeLocationWithinPage()
{
    m_client->dispatchDidChangeLocationWithinPage();
}

void FrameLoader::dispatchDidFinishLoadToClient()
{
    m_client->didFinishLoad();
}

void FrameLoader::updateGlobalHistoryForStandardLoad(const KURL& url)
{
    m_client->updateGlobalHistoryForStandardLoad(url);
}

void FrameLoader::updateGlobalHistoryForReload(const KURL& url)
{
    m_client->updateGlobalHistoryForReload(url);
}

bool FrameLoader::shouldGoToHistoryItem(HistoryItem* item) const
{
    return m_client->shouldGoToHistoryItem(item);
}

void FrameLoader::addExtraFieldsToRequest(ResourceRequest& request, bool mainResource, bool alwaysFromRequest)
{
    applyUserAgent(request);
    
    if (m_loadType == FrameLoadTypeReload) {
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "max-age=0");
    }
    
    // Don't set the cookie policy URL if it's already been set.
    if (request.mainDocumentURL().isEmpty()) {
        if (mainResource && (isLoadingMainFrame() || alwaysFromRequest))
            request.setMainDocumentURL(request.url());
        else if (Page* page = m_frame->page())
            request.setMainDocumentURL(page->mainFrame()->loader()->url());
    }
    
    if (mainResource)
        request.setHTTPAccept("text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5");
}

void FrameLoader::committedLoad(DocumentLoader* loader, const char* data, int length)
{
    m_client->committedLoad(loader, data, length);
}

void FrameLoader::post(const KURL& URL, const String& referrer, const String& frameName, PassRefPtr<FormData> formData, 
    const String& contentType, Event* event, HTMLFormElement* form, const HashMap<String, String>& formValues)
{
    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    ResourceRequest request(URL);
    addExtraFieldsToRequest(request, true, true);

    if (!referrer.isEmpty())
        request.setHTTPReferrer(referrer);
    request.setHTTPMethod("POST");
    request.setHTTPBody(formData);
    request.setHTTPContentType(contentType);

    NavigationAction action(URL, FrameLoadTypeStandard, true, event);

    RefPtr<FormState> formState;
    if (form && !formValues.isEmpty())
        formState = FormState::create(form, formValues, m_frame);

    if (!frameName.isEmpty()) {
        if (Frame* targetFrame = m_frame->tree()->find(frameName))
            targetFrame->loader()->load(request, action, FrameLoadTypeStandard, formState.release());
        else
            checkNewWindowPolicy(action, request, formState.release(), frameName);
    } else
        load(request, action, FrameLoadTypeStandard, formState.release());
}

bool FrameLoader::isReloading() const
{
    return documentLoader()->request().cachePolicy() == ReloadIgnoringCacheData;
}

void FrameLoader::loadEmptyDocumentSynchronously()
{
    ResourceRequest request(KURL(""));
    load(request);
}

void FrameLoader::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data)
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

    if (Page* page = m_frame->page())
        initialRequest.setMainDocumentURL(page->mainFrame()->loader()->documentLoader()->request().url());
    initialRequest.setHTTPUserAgent(client()->userAgent(request.url()));

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        didTellBridgeAboutLoad(newRequest.url().url());
        ResourceHandle::loadResourceSynchronously(newRequest, error, response, data);
    }
    
    sendRemainingDelegateMessages(identifier, response, data.size(), error);
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
    
    if (m_state == FrameStateProvisional) {
        KURL failedURL = m_provisionalDocumentLoader->originalRequestCopy().url();
        didNotOpenURL(failedURL);
            
        // We might have made a page cache item, but now we're bailing out due to an error before we ever
        // transitioned to the new page (before WebFrameState == commit).  The goal here is to restore any state
        // so that the existing view (that wenever got far enough to replace) can continue being used.
        m_frame->document()->setInPageCache(false);
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
    // FIXME:
    // some functions check m_quickRedirectComing, and others check for
    // FrameLoadTypeRedirectWithLockedHistory.  
    bool isRedirect = m_quickRedirectComing || m_policyLoadType == FrameLoadTypeRedirectWithLockedHistory;
    m_quickRedirectComing = false;

    if (!shouldContinue)
        return;

    KURL URL = request.url();
    
    m_documentLoader->replaceRequestURLForAnchorScroll(URL);
    if (!isRedirect && !shouldTreatURLAsSameAsCurrent(URL)) {
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
    
    scrollToAnchor(URL);
    
    if (!isRedirect)
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state.
        checkLoadComplete();
 
    dispatchDidChangeLocationWithinPage();
    m_client->didFinishLoad();
}

void FrameLoader::opened()
{
    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        updateHistoryForClientRedirect();

    if (m_documentLoader->isLoadingFromCachedPage()) {
        m_frame->document()->didRestoreFromCache();
        
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
        action, request, frameName);
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

    m_policyCheck.set(request, formState, function, argument);

    m_delegateIsDecidingNavigationPolicy = true;
    m_client->dispatchDecidePolicyForNavigationAction(&FrameLoader::continueAfterNavigationPolicy,
        action, request);
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

void FrameLoader::continueLoadAfterNavigationPolicy(const ResourceRequest& request, PassRefPtr<FormState> formState, bool shouldContinue)
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
                if (HistoryItem* resetItem = mainFrame->loader()->m_currentHistoryItem.get())
                    page->backForwardList()->goToItem(resetItem);
            }
        return;
    }

    FrameLoadType type = m_policyLoadType;
    stopAllLoaders();
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
    mainFrame->loader()->load(request, NavigationAction(), FrameLoadTypeStandard, formState);
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
        error = m_client->cancelledError(request);
    else
        error = ResourceError();

    request = newRequest;
}

void FrameLoader::loadedResourceFromMemoryCache(const ResourceRequest& request, const ResourceResponse& response, int length)
{
    if (dispatchDidLoadResourceFromMemoryCache(m_documentLoader.get(), request, response, length))
        return;

    unsigned long identifier;
    ResourceError error;
    ResourceRequest r(request);
    requestFromDelegate(r, identifier, error);
    sendRemainingDelegateMessages(identifier, response, length, error);
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
        cachedPage->setTimeStampToNow();
        cachedPage->setDocumentLoader(documentLoader());
        m_client->saveDocumentViewToCachedPage(cachedPage.get());

        pageCache()->add(item, cachedPage.release());
    }
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& URL) const
{
    if (!m_currentHistoryItem)
        return false;
    return URL == m_currentHistoryItem->url() || URL == m_currentHistoryItem->originalURL();
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

    LOG(History, "WebCoreHistory: Creating item for %s", url.url().ascii());
    
    // Frames that have never successfully loaded any content
    // may have no URL at all. Currently our history code can't
    // deal with such things, so we nip that in the bud here.
    // Later we may want to learn to live with nil for URL.
    // See bug 3368236 and related bugs for more information.
    if (url.isEmpty()) 
        url = KURL("about:blank");
    if (originalURL.isEmpty())
        originalURL = KURL("about:blank");
    
    RefPtr<HistoryItem> item = new HistoryItem(url, m_frame->tree()->name(), m_frame->tree()->parent() ? m_frame->tree()->parent()->tree()->name() : "", docLoader ? docLoader->title() : "");
    item->setOriginalURLString(originalURL.url());
    
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
    if (Page* page = m_frame->page())
        if (!documentLoader()->urlForHistory().isEmpty()) {
            Frame* mainFrame = page->mainFrame();
            ASSERT(mainFrame);
            FrameLoader* frameLoader = mainFrame->loader();

            if (!frameLoader->m_didPerformFirstNavigation && page->backForwardList()->entries().size() == 1) {
                frameLoader->m_didPerformFirstNavigation = true;
                m_client->didPerformFirstNavigation();
            }

            RefPtr<HistoryItem> item = frameLoader->createHistoryItemTree(m_frame, doClip);
            LOG(BackForward, "WebCoreBackForward - Adding backforward item %p for frame %s", item.get(), documentLoader()->URL().url().ascii());
            page->backForwardList()->addItem(item);
        }
}

PassRefPtr<HistoryItem> FrameLoader::createHistoryItemTree(Frame* targetFrame, bool clipAtTarget)
{
    RefPtr<HistoryItem> bfItem = createHistoryItem(m_frame->tree()->parent() ? true : false);
    if (m_previousHistoryItem)
        saveScrollPositionAndViewStateToItem(m_previousHistoryItem.get());
    if (!(clipAtTarget && m_frame == targetFrame)) {
        // save frame state for items that aren't loading (khtml doesn't save those)
        saveDocumentState();
        for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
            bfItem->addChildItem(child->loader()->createHistoryItemTree(targetFrame, clipAtTarget));
    }
    if (m_frame == targetFrame)
        bfItem->setIsTargetItem(true);
    return bfItem;
}

void FrameLoader::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    if (!item || !m_frame->view())
        return;
        
    item->setScrollPoint(IntPoint(m_frame->view()->contentsX(), m_frame->view()->contentsY()));
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
        if (!view->wasScrolledByUser()) {
            const IntPoint& scrollPoint = m_currentHistoryItem->scrollPoint();
            view->setContentsPos(scrollPoint.x(), scrollPoint.y());
        }
}

void FrameLoader::invalidateCurrentItemCachedPage()
{
    // When we are pre-commit, the currentItem is where the pageCache data resides    
    CachedPage* cachedPage = pageCache()->get(m_currentHistoryItem.get());

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    ASSERT(!cachedPage || cachedPage->document() == m_frame->document());
    if (cachedPage && cachedPage->document() == m_frame->document())
        cachedPage->clear();
    
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
        LOG(Loading, "WebCoreLoading %s: saving form state to %p", m_frame->tree()->name().domString().utf8().data(), item);
        item->setDocumentState(document->formElementsState());
    }
}

// Loads content into this frame, as specified by history item
void FrameLoader::loadItem(HistoryItem* item, FrameLoadType loadType)
{
    KURL itemURL = item->url();
    KURL itemOriginalURL = item->originalURL();
    KURL currentURL;
    if (documentLoader())
        currentURL = documentLoader()->URL();
    RefPtr<FormData> formData = item->formData();

    // Are we navigating to an anchor within the page?
    // Note if we have child frames we do a real reload, since the child frames might not
    // match our current frame structure, or they might not have the right content.  We could
    // check for all that as an additional optimization.
    // We also do not do anchor-style navigation if we're posting a form.
    
    if (!formData && !shouldReload(itemURL, currentURL) && urlsMatchItem(item)) {
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

        dispatchDidChangeLocationWithinPage();
        
        // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
        dispatchDidFinishLoadToClient();
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
                load(cachedPage->documentLoader(), loadType, 0);   
                inPageCache = true;
            } else {
                LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", m_provisionalHistoryItem->url().url().ascii());
                pageCache()->remove(item);
            }
        }
        
        if (!inPageCache) {
            ResourceRequest request(itemURL);

            addExtraFieldsToRequest(request, true, formData);

            // If this was a repost that failed the page cache, we might try to repost the form.
            NavigationAction action;
            if (formData) {
                request.setHTTPMethod("POST");
                request.setHTTPReferrer(item->formReferrer());
                request.setHTTPBody(formData);
                request.setHTTPContentType(item->formContentType());
        
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
                        request.setCachePolicy(ReloadIgnoringCacheData);
                        break;
                    case FrameLoadTypeBack:
                    case FrameLoadTypeForward:
                    case FrameLoadTypeIndexedBackForward:
                        if (itemURL.protocol() == "https")
                            request.setCachePolicy(ReturnCacheDataElseLoad);
                        break;
                    case FrameLoadTypeStandard:
                    case FrameLoadTypeRedirectWithLockedHistory:
                        // no-op: leave as protocol default
                        // FIXME:  I wonder if we ever hit this case
                        break;
                    case FrameLoadTypeSame:
                    case FrameLoadTypeReloadAllowingStaleData:
                    default:
                        ASSERT_NOT_REACHED();
                }

                action = NavigationAction(itemOriginalURL, loadType, false);
            }

            load(request, action, loadType, 0);
        }
    }
}

// Walk the frame tree and ensure that the URLs match the URLs in the item.
bool FrameLoader::urlsMatchItem(HistoryItem* item) const
{
    KURL currentURL = documentLoader()->URL();
    
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
    if (Page* page = m_frame->page())
        if (shouldGoToHistoryItem(targetItem)) {
            BackForwardList* bfList = page->backForwardList();
            HistoryItem* currentItem = bfList->currentItem();
            
            // Set the BF cursor before commit, which lets the user quickly click back/forward again.
            // - plus, it only makes sense for the top level of the operation through the frametree,
            // as opposed to happening for some/one of the page commits that might happen soon
            bfList->goToItem(targetItem);
            recursiveGoToItem(targetItem, currentItem, type);
        }
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
        currentURL = documentLoader()->URL();
    
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

void FrameLoader::addHistoryForCurrentLocation()
{
    if (!privateBrowsingEnabled()) {
        // FIXME: <rdar://problem/4880065> - This will be a hook into the WebCore global history, and this loader/client call will be removed
        updateGlobalHistoryForStandardLoad(documentLoader()->urlForHistory());
    }
    addBackForwardItemClippedAtTarget(true);
}

void FrameLoader::updateHistoryForStandardLoad()
{
    LOG(History, "WebCoreHistory: Updating History for Standard Load in frame %s", documentLoader()->URL().url().ascii());
    
    bool frameNavigationOnLoad = false;
    
    // if the navigation occured during on load and this is a subframe
    // update the current history item rather than adding a new one
    // <rdar://problem/5333496>
    if (m_navigationDuringLoad) {
        HTMLFrameOwnerElement* owner = m_frame->ownerElement();
        frameNavigationOnLoad = owner && !owner->createdByParser();
    }
    
    if (!frameNavigationOnLoad && !documentLoader()->isClientRedirect()) {
        if (!documentLoader()->urlForHistory().isEmpty())
            addHistoryForCurrentLocation();
    } else if (documentLoader()->unreachableURL().isEmpty() && m_currentHistoryItem) {
        m_currentHistoryItem->setURL(documentLoader()->URL());
        m_currentHistoryItem->setFormInfoFromRequest(documentLoader()->request());
    }
    
    // reset navigation during on load since we no longer
    // need it past thsi point
    m_navigationDuringLoad = false;
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
    
        if (loadType() == FrameLoadTypeReload)
            saveScrollPositionAndViewStateToItem(m_currentHistoryItem.get());
    
        // Sometimes loading a page again leads to a different result because of cookies. Bugzilla 4072
        if (documentLoader()->unreachableURL().isEmpty())
            m_currentHistoryItem->setURL(documentLoader()->requestURL());
    }
    
    // FIXME: <rdar://problem/4880065> - This will be a hook into the WebCore global history, and this loader/client call will be removed
    // Update the last visited time. Mostly interesting for URL autocompletion statistics.
    updateGlobalHistoryForReload(documentLoader()->originalURL());
}

void FrameLoader::updateHistoryForRedirectWithLockedHistory()
{
#if !LOG_DISABLED
    if (documentLoader())
        LOG(History, "WebCoreHistory: Updating History for internal load in frame %s", documentLoader()->title().utf8().data());
#endif
    
    if (documentLoader()->isClientRedirect()) {
        if (!m_currentHistoryItem && !m_frame->tree()->parent())
            addHistoryForCurrentLocation();
        if (m_currentHistoryItem) {
            m_currentHistoryItem->setURL(documentLoader()->URL());
            m_currentHistoryItem->setFormInfoFromRequest(documentLoader()->request());
        }
    } else {
        Frame* parentFrame = m_frame->tree()->parent();
        if (parentFrame && parentFrame->loader()->m_currentHistoryItem)
            parentFrame->loader()->m_currentHistoryItem->addChildItem(createHistoryItem(true));
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
        (type == FrameLoadTypeReload && !provisionalDocumentLoader()->unreachableURL().isEmpty())) {
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

void FrameLoader::mainReceivedCompleteError(DocumentLoader* loader, const ResourceError& error)
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
    return m_client->cancelledError(request);
}

ResourceError FrameLoader::blockedError(const ResourceRequest& request) const
{
    return m_client->blockedError(request);
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
    return activeDocumentLoader()->initialRequest().url();
}

String FrameLoader::referrer() const
{
    return documentLoader()->request().httpReferrer();
}

void FrameLoader::dispatchWindowObjectAvailable()
{
    Settings* settings = m_frame->settings();
    if (!settings || !settings->isJavaScriptEnabled() || !m_frame->scriptProxy()->haveInterpreter())
        return;

    m_client->windowObjectCleared();
    if (Page* page = m_frame->page())
        if (InspectorController* inspector = page->parentInspectorController())
            inspector->windowScriptObjectAvailable();
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize& size, Element* element, const HashMap<String, String>& args)
{
    String baseURLString;
    Vector<String> paramNames;
    Vector<String> paramValues;
    HashMap<String, String>::const_iterator end = args.end();
    for (HashMap<String, String>::const_iterator it = args.begin(); it != end; ++it) {
        if (it->first.lower() == "baseurl")
            baseURLString = it->second;
        paramNames.append(it->first);
        paramValues.append(it->second);
    }
    
    if (baseURLString.isEmpty())
        baseURLString = m_frame->document()->baseURL();
    KURL baseURL = completeURL(baseURLString);

    Widget* widget = m_client->createJavaAppletWidget(size, element, baseURL, paramNames, paramValues);
    if(widget && m_frame->view())
        m_frame->view()->addChild(widget);
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
    // This avoids an allocation of another String and the HashSet containts()
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

void FrameLoader::dispatchDidCommitLoad()
{
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
    m_client->dispatchWillSendRequest(loader, identifier, request, redirectResponse);

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

bool FrameLoader::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& response, int length)
{
    bool result = m_client->dispatchDidLoadResourceFromMemoryCache(loader, request, response, length);

    if (Page* page = m_frame->page())
        page->inspectorController()->didLoadResourceFromMemoryCache(loader, request, response, length);

    return result;
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
            cache->ref(this);
            return true;
        case CachedResource::ImageResource:
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
        (*it)->deref(this);
    m_externalRequestsInLowBandwidthDisplay.clear();
}
    
void FrameLoader::notifyFinished(CachedResource* script)
{
    HashSet<CachedResource*>::iterator it = m_externalRequestsInLowBandwidthDisplay.find(script);
    if (it != m_externalRequestsInLowBandwidthDisplay.end()) {
        (*it)->deref(this);
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
            if (m_frame->scriptProxy())
                m_frame->scriptProxy()->clear();
            if (m_frame->view())
                m_frame->view()->clear();
            
            // similar to begin(), should be refactored to share more code
            RefPtr<Document> newDoc = DOMImplementation::instance()->
                createDocument(m_responseMIMEType, m_frame->view(), m_frame->inViewSourceMode());
            m_frame->setDocument(newDoc);
            newDoc->setURL(m_URL.url());
            newDoc->setBaseURL(m_URL.url());
            if (m_decoder)
                newDoc->setDecoder(m_decoder.get());
            restoreDocumentState();
            partClearedInBegin();         
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
                // set cachePolicy to Cache to use the loaded resource
                newDoc->docLoader()->setCachePolicy(CachePolicyCache);
                newDoc->determineParseMode(m_pendingSourceInLowBandwidthDisplay);
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

} // namespace WebCore
