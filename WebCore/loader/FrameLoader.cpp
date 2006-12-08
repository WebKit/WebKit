/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "Cache.h"
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
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLPlugInElement.h"
#include "HTTPParsers.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "PageState.h"
#include "RenderPart.h"
#include "RenderWidget.h"
#include "ResourceRequest.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "SystemTime.h"
#include "TextResourceDecoder.h"
#include "WindowFeatures.h"
#include "XMLTokenizer.h"
#include "kjs_binding.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "xmlhttprequest.h"
#include <kjs/JSLock.h>
#include <kjs/object.h>

#if PLATFORM(MAC)
#include "FrameMac.h"
#endif

using namespace KJS;

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;

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

    ScheduledRedirection(double redirectDelay, const String& redirectURL, bool redirectLockHistory)
        : type(redirection)
        , delay(redirectDelay)
        , URL(redirectURL)
        , historySteps(0)
        , lockHistory(redirectLockHistory)
        , wasUserGesture(false)
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

static void cancelAll(const ResourceLoaderSet& loaders)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->cancel();
}

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
        case FrameLoadTypeInternal:
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
    if (document)
        return cache()->loader()->numRequests(document->docLoader());
    return 0;
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
    , m_cachePolicy(CachePolicyVerify)
    , m_isExecutingJavaScriptFormAction(false)
    , m_isRunningScript(false)
    , m_wasLoadEventEmitted(false)
    , m_wasUnloadEventEmitted(false)
    , m_isComplete(false)
    , m_isLoadingMainResource(false)
    , m_cancellingWithLoadInProgress(false)
    , m_needsClear(false)
    , m_receivedData(false)
    , m_encodingWasChosenByUser(false)
    , m_containsPlugIns(false)
    , m_redirectionTimer(this, &FrameLoader::redirectionTimerFired)
    , m_opener(0)
    , m_openedByJavaScript(false)
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

static void setAllDefersLoading(const ResourceLoaderSet& loaders, bool defers)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->setDefersLoading(defers);
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->setDefersLoading(defers);
    setAllDefersLoading(m_subresourceLoaders, defers);
    setAllDefersLoading(m_plugInStreamLoaders, defers);
    m_client->setDefersLoading(defers);
}

Frame* FrameLoader::createWindow(const FrameLoadRequest& request, const WindowFeatures& features)
{ 
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty())
        if (Frame* frame = m_frame->tree()->find(request.frameName())) {
#if PLATFORM(MAC)
            if (!request.resourceRequest().url().isEmpty())
                frame->loader()->load(request, true, 0, 0, HashMap<String, String>());
#endif
            frame->page()->chrome()->focus();
            return frame;
        }

    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    requestWithReferrer.resourceRequest().setHTTPReferrer(m_outgoingReferrer);
    
    Page* page;
    if (features.dialog)
        page = m_frame->page()->chrome()->createModalDialog(requestWithReferrer);
    else
        page = m_frame->page()->chrome()->createWindow(requestWithReferrer);
    if (!page)
        return 0;

    Frame* frame = page->mainFrame();
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

    return frame;
}

bool FrameLoader::canHandleRequest(const ResourceRequest& request)
{
    return m_client->canHandleRequest(request);
}

void FrameLoader::changeLocation(const String& URL, const String& referrer, bool lockHistory, bool userGesture)
{
    if (URL.find("javascript:", 0, false) == 0) {
        String script = KURL::decode_string(URL.substring(strlen("javascript:")).deprecatedString());
        JSValue* result = executeScript(0, script, userGesture);
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
    ResourceRequest request(completeURL(URL), referrer, policy);
    
    urlSelected(request, "_self", 0, lockHistory);
}

void FrameLoader::urlSelected(const ResourceRequest& request, const String& _target, Event* triggeringEvent, bool lockHistory)
{
    String target = _target;
    if (target.isEmpty() && m_frame->document())
        target = m_frame->document()->baseTarget();

    const KURL& url = request.url();
    if (url.url().startsWith("javascript:", false)) {
        executeScript(0, KURL::decode_string(url.url().mid(strlen("javascript:"))), true);
        return;
    }

    if (!url.isValid())
        return;

    FrameLoadRequest frameRequest(request, target);

    if (frameRequest.resourceRequest().httpReferrer().isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    urlSelected(frameRequest, triggeringEvent);
}

bool FrameLoader::requestFrame(Element* ownerElement, const String& urlString, const AtomicString& frameName)
{
    // Support for <frame src="javascript:string">
    KURL scriptURL;
    KURL url;
    if (urlString.startsWith("javascript:", false)) {
        scriptURL = urlString.deprecatedString();
        url = "about:blank";
    } else
        url = completeURL(urlString);

    Frame* frame = m_frame->tree()->child(frameName);
    if (frame) {
        ResourceRequestCachePolicy policy = (m_cachePolicy == CachePolicyReload) || (m_cachePolicy == CachePolicyRefresh)
            ? ReloadIgnoringCacheData : UseProtocolCachePolicy;
        frame->loader()->urlSelected(ResourceRequest(url, m_outgoingReferrer, policy), 0);
    } else
        frame = loadSubframe(ownerElement, url, frameName, m_outgoingReferrer);
    
    if (!frame)
        return false;

    if (!scriptURL.isEmpty())
        frame->loader()->replaceContentsWithScriptResult(scriptURL);

    return true;
}

Frame* FrameLoader::loadSubframe(Element* ownerElement, const KURL& url, const String& name, const String& referrer)
{
    Frame* frame = createFrame(url, name, ownerElement, referrer);
    if (!frame)  {
        checkEmitLoadEvent();
        return 0;
    }
    
    frame->loader()->m_isComplete = false;
    
    if (ownerElement->renderer() && frame->view())
        static_cast<RenderWidget*>(ownerElement->renderer())->setWidget(frame->view());
    
    checkEmitLoadEvent();
    
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
    if (!u.isValid())
        return;

    DeprecatedString urlString = u.url();
    if (urlString.startsWith("javascript:", false)) {
        m_isExecutingJavaScriptFormAction = true;
        executeScript(0, KURL::decode_string(urlString.mid(strlen("javascript:"))));
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
        DeprecatedString encodedBody;
        if (equalIgnoringCase(contentType, "multipart/form-data"))
            // FIXME: is this correct? I suspect not, but what site can we test this on?
            encodedBody = KURL::encode_string(formData->flattenToString().deprecatedString());
        else if (equalIgnoringCase(contentType, "text/plain")) {
            // Convention seems to be to decode, and s/&/\n/
            encodedBody = formData->flattenToString().deprecatedString();
            encodedBody.replace('&', '\n');
            encodedBody.replace('+', ' ');
            encodedBody = KURL::decode_string(encodedBody); // Decode the rest of it
            encodedBody = KURL::encode_string(encodedBody); // Recode for the URL
        } else
            encodedBody = KURL::encode_string(formData->flattenToString().deprecatedString());

        DeprecatedString query = u.query();
        if (!query.isEmpty())
            query += '&';
        query += "body=";
        query += encodedBody;
        u.setQuery(query);
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
  
    m_responseRefreshHeader = String();
    m_responseModifiedHeader = String();

    if (sendUnload) {
        if (m_frame->document()) {
            if (m_wasLoadEventEmitted && !m_wasUnloadEventEmitted) {
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
    m_wasLoadEventEmitted = true; // don't want that one either
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
    m_wasLoadEventEmitted = false;

    m_frame->d->m_kjsStatusBarText = String();
    m_frame->d->m_kjsDefaultStatusBarText = String();

    m_frame->d->m_bJScriptEnabled = m_frame->settings()->isJavaScriptEnabled();
    m_frame->d->m_bJavaEnabled = m_frame->settings()->isJavaEnabled();
    m_frame->d->m_bPluginsEnabled = m_frame->settings()->isPluginsEnabled();

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
    m_wasLoadEventEmitted = false;

    // Prevent window.open(url) -- eg window.open("about:blank") -- from blowing away results
    // from a subsequent window.document.open / window.document.write call. 
    // Cancelling redirection here works for all cases because document.open 
    // implicitly precedes document.write.
    cancelRedirection(); 
    m_URL = m_frame->document()->URL();
}

void FrameLoader::replaceContentsWithScriptResult(const KURL& url)
{
    JSValue* result = executeScript(0, KURL::decode_string(url.url().mid(strlen("javascript:"))));
    String scriptResult;
    if (!getString(result, scriptResult))
        return;
    begin();
    write(scriptResult);
    end();
}

JSValue* FrameLoader::executeScript(Node* node, const String& script, bool forceUserGesture)
{
    return executeScript(forceUserGesture ? String() : String(m_URL.url()), 0, node, script);
}

JSValue* FrameLoader::executeScript(const String& URL, int baseLine, Node* node, const String& script)
{
    KJSProxy* proxy = m_frame->scriptProxy();
    if (!proxy)
        return 0;

    bool wasRunningScript = m_isRunningScript;
    m_isRunningScript = true;

    JSValue* result = proxy->evaluate(URL, baseLine, script, node);

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

void FrameLoader::clear(bool clearWindowProperties)
{
    // FIXME: Commenting out the below line causes <http://bugs.webkit.org/show_bug.cgi?id=11212>, but putting it
    // back causes a measurable performance regression which we will need to fix to restore the correct behavior
    // urlsBridgeKnowsAbout.clear();

#if PLATFORM(MAC)
    Mac(m_frame)->setMarkedTextRange(0, nil, nil);
#endif

    if (!m_needsClear)
        return;
    m_needsClear = false;

#if !PLATFORM(MAC)
    // FIXME: Remove this after making other platforms do loading more like Mac.
    detachChildren();
#endif

    if (m_frame->document()) {
        m_frame->document()->cancelParsing();
        m_frame->document()->detach();
    }

    // Do this after detaching the document so that the unload event works.
    if (clearWindowProperties && m_frame->scriptProxy())
        m_frame->scriptProxy()->clear();

    m_frame->selectionController()->clear();
    m_frame->eventHandler()->clear();
    if (m_frame->view())
        m_frame->view()->clear();

    // Do not drop the document before the script proxy and view are cleared, as some destructors
    // might still try to access the document.
    m_frame->d->m_doc = 0;
    m_decoder = 0;

    m_containsPlugIns = false;
    m_frame->cleanupPluginObjects();
  
    m_redirectionTimer.stop();
    m_scheduledRedirection.clear();

    m_receivedData = false;

    if (!m_encodingWasChosenByUser)
        m_encoding = String();
}

void FrameLoader::receivedFirstData()
{
    begin(m_workingURL);

    m_frame->document()->docLoader()->setCachePolicy(m_cachePolicy);
    m_workingURL = KURL();

    const String& refresh = m_responseRefreshHeader;

    double delay;
    String URL;

    if (!parseHTTPRefresh(refresh, delay, URL))
        return;

    if (URL.isEmpty())
        URL = m_URL.url();
    else
        URL = m_frame->document()->completeURL(URL);

    // We want a new history item if the refresh timeout > 1 second
    scheduleRedirection(delay, URL, delay <= 1);
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

void FrameLoader::begin(const KURL& url)
{
    if (m_workingURL.isEmpty())
        createEmptyDocument(); // Creates an empty document if we don't have one already

    clear();
    partClearedInBegin();

    m_needsClear = true;
    m_isComplete = false;
    m_wasLoadEventEmitted = false;
    m_isLoadingMainResource = true;

    KURL ref(url);
    ref.setUser(DeprecatedString());
    ref.setPass(DeprecatedString());
    ref.setRef(DeprecatedString());
    m_outgoingReferrer = ref.url();
    m_URL = url;
    KURL baseurl;

    if (!m_URL.isEmpty())
        baseurl = m_URL;

    RefPtr<Document> document = DOMImplementation::instance()->
        createDocument(m_responseMIMEType, m_frame->view(), m_frame->inViewSourceMode());
    m_frame->d->m_doc = document;

    if (!document->attached())
        document->attach();
    document->setURL(m_URL.url());
    // We prefer m_baseURL over m_URL because m_URL changes when we are
    // about to load a new page.
    document->setBaseURL(baseurl.url());
    if (m_decoder)
        document->setDecoder(m_decoder.get());

    updatePolicyBaseURL();

    document->docLoader()->setAutoLoadImages(m_frame->settings()->autoLoadImages());

    const KURL& userStyleSheet = m_frame->settings()->userStyleSheetLocation();
    if (!userStyleSheet.isEmpty())
        m_frame->setUserStyleSheetLocation(KURL(userStyleSheet));

    restoreDocumentState();

    document->implicitOpen();

    if (m_frame->view())
        m_frame->view()->resizeContents(0, 0);
}

void FrameLoader::write(const char* str, int len)
{
    if (len == 0)
        return;
    
    if (len == -1)
        len = strlen(str);

    Tokenizer* tokenizer = m_frame->document()->tokenizer();
    if (tokenizer && tokenizer->wantsRawData()) {
        tokenizer->writeRawData(str, len);
        return;
    }
    
    if (!m_decoder) {
        m_decoder = new TextResourceDecoder(m_responseMIMEType, m_frame->settings()->encoding());
        if (!m_encoding.isNull())
            m_decoder->setEncoding(m_encoding,
                m_encodingWasChosenByUser ? TextResourceDecoder::UserChosenEncoding : TextResourceDecoder::EncodingFromHTTPHeader);
        if (m_frame->document())
            m_frame->document()->setDecoder(m_decoder.get());
    }

    String decoded = m_decoder->decode(str, len);
    if (decoded.isEmpty())
        return;

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
    endIfNotLoading();
}

void FrameLoader::endIfNotLoading()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(), 
    // so we'll add a protective refcount
    RefPtr<Frame> protector(m_frame);

    if (m_isLoadingMainResource)
        return;

    // make sure nothing's left in there
    if (m_frame->document()) {
        if (m_decoder) {
            String decoded = m_decoder->flush();
            if (!m_receivedData) {
                m_receivedData = true;
                m_frame->document()->determineParseMode(decoded);
            }
            write(decoded);
        }
        m_frame->document()->finishParsing();
    } else
        // WebKit partially uses WebCore when loading non-HTML docs.  In these cases doc==nil, but
        // WebCore is enough involved that we need to checkCompleted() in order for m_bComplete to
        // become true.  An example is when a subframe is a pure text doc, and that subframe is the
        // last one to complete.
        checkCompleted();

    startIconLoader();
}

void FrameLoader::startIconLoader()
{
    // FIXME: We kick off the icon loader when the frame is done receiving its main resource.
    // But we should instead do it when we're done parsing the head element.
    if (!isLoadingMainFrame())
        return;

    IconDatabase* iconDB = IconDatabase::sharedIconDatabase();
    if (!iconDB)
        return;
    if (!iconDB->enabled())
        return;

    KURL url(iconURL());
    String urlString(url.url());
    if (urlString.isEmpty())
        return;

    // If we already have an unexpired icon, we won't kick off a load but we *will* map the appropriate URLs to it
    if (iconDB->hasEntryForIconURL(urlString) && loadType() != FrameLoadTypeReload && !iconDB->isIconExpiredForIconURL(urlString)) {
        commitIconURLToIconDatabase(url);
        return;
    }

    if (!m_iconLoader)
        m_iconLoader.set(IconLoader::create(m_frame).release());
    m_iconLoader->startLoading();
}

void FrameLoader::commitIconURLToIconDatabase(const KURL& icon)
{
    IconDatabase* iconDB = IconDatabase::sharedIconDatabase();
    ASSERT(iconDB);
    iconDB->setIconURLForPageURL(icon.url(), m_URL.url());
    iconDB->setIconURLForPageURL(icon.url(), originalRequestURL().url());
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
    // This can be called from the Frame's destructor, in which case we shouldn't protect ourselves
    // because doing so will cause us to re-enter the destructor when protector goes out of scope.
    RefPtr<Frame> protector = m_frame->refCount() > 0 ? m_frame : 0;

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
    if (m_frame->document() && m_frame->document()->docLoader())
        if (cache()->loader()->numRequests(m_frame->document()->docLoader()))
            return;

    // OK, completed.
    m_isComplete = true;

    checkEmitLoadEvent(); // if we didn't do it before

    // Do not start a redirection timer for subframes here.
    // That is deferred until the parent is completed.
    if (m_scheduledRedirection && !m_frame->tree()->parent())
        startRedirectionTimer();

    completed();
}

void FrameLoader::checkEmitLoadEvent()
{
    if (m_wasLoadEventEmitted || !m_frame->document() || m_frame->document()->parsing())
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
                child->document()->setDomain(domain);
    }

    m_wasLoadEventEmitted = true;
    m_wasUnloadEventEmitted = false;
    if (m_frame->document())
        m_frame->document()->implicitClose();
}

KURL FrameLoader::baseURL() const
{
    if (!m_frame->document())
        return KURL();
    return m_frame->document()->baseURL();
}

String FrameLoader::baseTarget() const
{
    if (!m_frame->document())
        return String();
    return m_frame->document()->baseTarget();
}

KURL FrameLoader::completeURL(const String& url)
{
    if (!m_frame->document())
        return url.deprecatedString();
    return m_frame->document()->completeURL(url).deprecatedString();
}

void FrameLoader::scheduleRedirection(double delay, const String& url, bool doLockHistory)
{
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (!m_scheduledRedirection || delay <= m_scheduledRedirection->delay)
        scheduleRedirection(new ScheduledRedirection(delay, url, doLockHistory));
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
    bool duringLoad = !m_frame->document();

    // If a redirect was scheduled during a load, then stop the current load.
    // Otherwise when the current load transitions from a provisional to a 
    // committed state, pending redirects may be cancelled. 
    if (duringLoad)
        stopLoading(true);   

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

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the navigation.
    if (equalIgnoringRef(m_URL, historyURL(steps))) {
        goBackOrForward(steps);
        return;
    }
    
    scheduleRedirection(new ScheduledRedirection(steps));
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
                urlSelected(m_URL, "", 0);
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
    return m_frame->settings()->encoding();
}

bool FrameLoader::gotoAnchor(const String& name)
{
    if (!m_frame->document())
        return false;

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

    KURL completedURL;
    if (!url.isEmpty())
        completedURL = completeURL(url);

    bool useFallback;
    if (shouldUsePlugin(renderer->element(), completedURL, mimeType, renderer->hasFallbackContent(), useFallback)) {
        
        if (!m_frame->pluginsEnabled())
            return false;
        
        return loadPlugin(renderer, completedURL, mimeType, paramNames, paramValues, useFallback);
    }

    ASSERT(renderer->node()->hasTagName(objectTag) || renderer->node()->hasTagName(embedTag));
    AtomicString uniqueFrameName = m_frame->tree()->uniqueChildName(frameName);
    static_cast<HTMLPlugInElement*>(renderer->node())->setFrameName(uniqueFrameName);
    
    // FIXME: OK to always make a new one? When does the old frame get removed?
    return loadSubframe(static_cast<Element*>(renderer->node()), completedURL, uniqueFrameName, m_outgoingReferrer);
}

bool FrameLoader::shouldUsePlugin(Node* element, const KURL& url, const String& mimeType, bool hasFallback, bool& useFallback)
{
    ObjectContentType objectType = objectContentType(url, mimeType);
    // If an object's content can't be handled and it has no fallback, let
    // it be handled as a plugin to show the broken plugin icon.
    useFallback = objectType == ObjectContentNone && hasFallback;
    return objectType == ObjectContentNone || objectType == ObjectContentPlugin;
}

bool FrameLoader::loadPlugin(RenderPart* renderer, const KURL& url, const String& mimeType, 
    const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    Widget* widget = 0;

    if (renderer && !useFallback) {
        Element* pluginElement = 0;
        if (renderer->node() && renderer->node()->isElementNode())
            pluginElement = static_cast<Element*>(renderer->node());

        widget = createPlugin(pluginElement, url, paramNames, paramValues, mimeType);
        if (widget) {
            renderer->setWidget(widget);
            m_containsPlugIns = true;
        }
    }

    checkEmitLoadEvent();
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

String FrameLoader::lastModified() const
{
    return m_responseModifiedHeader;
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
}

bool FrameLoader::openedByJavaScript()
{
    return m_openedByJavaScript;
}

void FrameLoader::setOpenedByJavaScript()
{
    m_openedByJavaScript = true;
}

void FrameLoader::handleFallbackContent()
{
    Element* owner = m_frame->ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElement*>(owner)->renderFallbackContent();
}

void FrameLoader::provisionalLoadStarted()
{
    m_firstLayoutDone = false;
    cancelRedirection(true);
    m_client->provisionalLoadStarted();
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

#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
void FrameLoader::didNotOpenURL(const KURL& URL)
{
    if (m_submittedFormURL == URL)
        m_submittedFormURL = KURL();
}

void FrameLoader::resetMultipleFormSubmissionProtection()
{
    m_submittedFormURL = KURL();
}
#endif

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
    return m_frame->document()
        && !m_frame->tree()->childCount()
        && !m_frame->tree()->parent()
        && !m_containsPlugIns
        && !m_URL.protocol().startsWith("https")
        && !m_frame->document()->applets()->length()
        && !m_frame->document()->hasWindowEventListener(unloadEvent)
        && !m_frame->document()->hasPasswordField();
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

bool FrameLoader::isLoadingMainResource() const
{
    return m_isLoadingMainResource;
}

KURL FrameLoader::url() const
{
    return m_URL;
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

void FrameLoader::updateBaseURLForEmptyDocument()
{
    Element* owner = m_frame->ownerElement();
    // FIXME: Should embed be included?
    if (owner && (owner->hasTagName(iframeTag) || owner->hasTagName(objectTag) || owner->hasTagName(embedTag)))
        m_frame->document()->setBaseURL(m_frame->tree()->parent()->document()->baseURL());
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
    m_client->progressStarted();
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

#if PLATFORM(MAC)
void FrameLoader::load(const KURL& URL, Event* event)
{
    load(ResourceRequest(URL), true, event, 0, HashMap<String, String>());
}
#endif

bool FrameLoader::canTarget(Frame* target) const
{
    // This method prevents this exploit:
    // <rdar://problem/3715785> multiple frame injection vulnerability reported by Secunia, affects almost all browsers

    if (!target)
        return true;

    // Allow with navigation within the same page/frameset.
    if (m_frame->page() == target->page())
        return true;

    String domain;
    if (Document* document = m_frame->document())
        domain = document->domain();
    // Allow if the request is made from a local file.
    if (domain.isEmpty())
        return true;
    
    Frame* parent = target->tree()->parent();
    // Allow if target is an entire window.
    if (!parent)
        return true;
    
    String parentDomain;
    if (Document* parentDocument = parent->document())
        domain = parentDocument->domain();
    // Allow if the domain of the parent of the targeted frame equals this domain.
    return equalIgnoringCase(parentDomain, domain);
}

void FrameLoader::stopLoadingPlugIns()
{
    cancelAll(m_plugInStreamLoaders);
}

void FrameLoader::stopLoadingSubresources()
{
    cancelAll(m_subresourceLoaders);
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

void FrameLoader::cancelMainResourceLoad()
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->cancel();
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

void FrameLoader::addPlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.add(loader);
    activeDocumentLoader()->setLoading(true);
}

void FrameLoader::removePlugInStreamLoader(ResourceLoader* loader)
{
    m_plugInStreamLoaders.remove(loader);
    activeDocumentLoader()->updateLoading();
}

bool FrameLoader::hasMainResourceLoader() const
{
    return m_mainResourceLoader != 0;
}

bool FrameLoader::isLoadingSubresources() const
{
    return !m_subresourceLoaders.isEmpty();
}

bool FrameLoader::isLoadingPlugIns() const
{
    return !m_plugInStreamLoaders.isEmpty();
}

bool FrameLoader::isLoading() const
{
    return isLoadingMainResource() || isLoadingSubresources() || isLoadingPlugIns();
}

void FrameLoader::addSubresourceLoader(ResourceLoader* loader)
{
    ASSERT(!m_provisionalDocumentLoader);
    m_subresourceLoaders.add(loader);
    activeDocumentLoader()->setLoading(true);
}

void FrameLoader::removeSubresourceLoader(ResourceLoader* loader)
{
    m_subresourceLoaders.remove(loader);
    activeDocumentLoader()->updateLoading();
    checkLoadComplete();
}

void FrameLoader::releaseMainResourceLoader()
{
    m_mainResourceLoader = 0;
}

void FrameLoader::setDocumentLoader(DocumentLoader* loader)
{
    if (!loader && !m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    m_client->prepareForDataSourceReplacement();
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
    m_client->progressCompleted();
    setState(FrameStateComplete);
}

void FrameLoader::markLoadComplete()
{
    setState(FrameStateComplete);
}

void FrameLoader::commitProvisionalLoad()
{
    stopLoadingSubresources();
    stopLoadingPlugIns();

    setDocumentLoader(m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(0);
    setState(FrameStateCommittedPage);
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

void FrameLoader::open(PageState& state)
{
    ASSERT(m_frame->page()->mainFrame() == m_frame);

    cancelRedirection();

    // We still have to close the previous part page.
    closeURL();

    m_isComplete = false;
    
    // Don't re-emit the load event.
    m_wasLoadEventEmitted = true;
    
    // Delete old status bar messages (if it _was_ activated on last URL).
    if (m_frame->javaScriptEnabled()) {
        m_frame->d->m_kjsStatusBarText = String();
        m_frame->d->m_kjsDefaultStatusBarText = String();
    }

    KURL URL = state.URL();

    if (URL.protocol().startsWith("http") && !URL.host().isEmpty() && URL.path().isEmpty())
        URL.setPath("/");
    
    m_URL = URL;
    m_workingURL = URL;

    started();

    clear();

    Document* document = state.document();
    document->setInPageCache(false);

    m_needsClear = true;
    m_isComplete = false;
    m_wasLoadEventEmitted = false;
    m_outgoingReferrer = URL.url();
    
    m_frame->setView(document->view());
    
    m_frame->d->m_doc = document;
    m_decoder = document->decoder();

    updatePolicyBaseURL();

    state.restore(m_frame->page());

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

#if PLATFORM(MAC)
    RefPtr<DocumentLoader> dl = activeDocumentLoader();
    dl->finishedLoading();
    if (dl->mainDocumentError() || !dl->frameLoader())
        return;
    dl->setPrimaryLoadComplete(true);
    m_client->dispatchDidLoadMainResource(dl.get());
#endif
    checkLoadComplete();
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
    Element* owner = m_frame->ownerElement();
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
    check.clearRequest();
    check.call();
}

void FrameLoader::continueAfterContentPolicy(PolicyAction policy)
{
    PolicyCheck check = m_policyCheck;
    m_policyCheck.clear();
    check.call(policy);
}

#if PLATFORM(MAC)
void FrameLoader::continueAfterWillSubmitForm(PolicyAction)
{
    startLoading();
}
#endif

#if PLATFORM(MAC)
void FrameLoader::didFirstLayout()
{
    if (isBackForwardLoadType(m_loadType) && m_client->hasBackForwardList())
        m_client->restoreScrollPositionAndViewState();

    m_firstLayoutDone = true;
    m_client->dispatchDidFirstLayout();
}
#endif

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

void FrameLoader::closeDocument()
{
    m_client->willCloseDocument();
    closeURL();
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

// Called every time a resource is completely loaded, or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT(m_client->hasWebView());
    for (RefPtr<Frame> frame = m_frame; frame; frame = frame->tree()->parent())
        frame->loader()->checkLoadCompleteForThisFrame();
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

#if PLATFORM(MAC)
void FrameLoader::submitForm(const FrameLoadRequest& request, Event* event)
{
#ifdef MULTIPLE_FORM_SUBMISSION_PROTECTION
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
#endif

    // FIXME: Why do we always pass true for userGesture?
    load(request, true, event, m_formAboutToBeSubmitted.get(), m_formValuesAboutToBeSubmitted);

    clearRecordedFormValues();
}
#endif

#if PLATFORM(MAC)
void FrameLoader::urlSelected(const FrameLoadRequest& request, Event* event)
{
    FrameLoadRequest copy = request;
    if (copy.resourceRequest().httpReferrer().isEmpty())
        copy.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    // FIXME: Why do we always pass true for userGesture?
    load(copy, true, event, 0, HashMap<String, String>());
}
#endif

String FrameLoader::userAgent() const
{
    return m_client->userAgent();
}

void FrameLoader::createEmptyDocument()
{
    // Although it's not completely clear from the name of this function,
    // it does nothing if we already have a document, and just creates an
    // empty one if we have no document at all.
#if PLATFORM(MAC)
    if (!m_frame->document()) {
        loadEmptyDocumentSynchronously();
        updateBaseURLForEmptyDocument();
    }
#elif PLATFORM(QT)
    if (!m_frame->document()) {
        begin();
        end();
    }
#endif
}

void FrameLoader::tokenizerProcessedData()
{
    if (m_frame->document())
        checkCompleted();
    checkLoadComplete();
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

    closeDocument();
    stopAllLoaders();
    m_client->detachedFromParent1();
    detachChildren();
    m_client->detachedFromParent2();
    setDocumentLoader(0);
    m_client->detachedFromParent3();
    if (Frame* parent = m_frame->tree()->parent())
        parent->tree()->removeChild(m_frame);
    m_frame->setView(0);
#if PLATFORM(MAC)
    closeBridge();
#endif
    m_client->detachedFromParent4();
}

} // namespace WebCore
