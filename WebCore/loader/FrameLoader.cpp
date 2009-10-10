/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "ApplicationCacheHost.h"
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
#include "HTMLAppletElement.h"
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
#include "PageTransitionEvent.h"
#include "PlaceholderDocument.h"
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
#include "ScriptString.h"
#include "ScriptValue.h"
#include "SecurityOrigin.h"
#include "SegmentedString.h"
#include "Settings.h"

#if ENABLE(SHARED_WORKERS)
#include "SharedWorkerRepository.h"
#endif

#include "TextResourceDecoder.h"
#include "WindowFeatures.h"
#include "XMLHttpRequest.h"
#include "XMLTokenizer.h"
#include "XSSAuditor.h"
#include <wtf/CurrentTime.h>
#include <wtf/StdLibExtras.h>


#if ENABLE(SVG)
#include "SVGDocument.h"
#include "SVGLocatable.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGViewElement.h"
#include "SVGViewSpec.h"
#endif

#if PLATFORM(MAC) || PLATFORM(WIN)
#define PAGE_CACHE_ACCEPTS_UNLOAD_HANDLERS
#endif

namespace WebCore {

#if ENABLE(SVG)
using namespace SVGNames;
#endif
using namespace HTMLNames;

#if ENABLE(XHTMLMP)
static const char defaultAcceptHeader[] = "application/xml,application/vnd.wap.xhtml+xml,application/xhtml+xml;profile='http://www.wapforum.org/xhtml',text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5";
#else
static const char defaultAcceptHeader[] = "application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5";
#endif
static double storedTimeOfLastCompletedLoad;

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
        case FrameLoadTypeBackWMLDeckNotAccessible:
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

static inline bool canReferToParentFrameEncoding(const Frame* frame, const Frame* parentFrame) 
{
    return parentFrame && parentFrame->document()->securityOrigin()->canAccess(frame->document()->securityOrigin());
}

HistoryController::HistoryController(Frame* frame)
    : m_frame(frame)
{
}

HistoryController::~HistoryController()
{
}

FrameLoader::FrameLoader(Frame* frame, FrameLoaderClient* client)
    : m_frame(frame)
    , m_client(client)
    , m_policyChecker(frame)
    , m_history(frame)
    , m_state(FrameStateCommittedPage)
    , m_loadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_firstLayoutDone(false)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_inStopAllLoaders(false)
    , m_isExecutingJavaScriptFormAction(false)
    , m_didCallImplicitClose(false)
    , m_wasUnloadEventEmitted(false)
    , m_unloadEventBeingDispatched(false)
    , m_isComplete(false)
    , m_isLoadingMainResource(false)
    , m_needsClear(false)
    , m_receivedData(false)
    , m_encodingWasChosenByUser(false)
    , m_containsPlugIns(false)
    , m_checkTimer(this, &FrameLoader::checkTimerFired)
    , m_shouldCallCheckCompleted(false)
    , m_shouldCallCheckLoadComplete(false)
    , m_opener(0)
    , m_creatingInitialEmptyDocument(false)
    , m_isDisplayingInitialEmptyDocument(false)
    , m_committedFirstRealDocumentLoad(false)
    , m_didPerformFirstNavigation(false)
    , m_loadingFromCachedPage(false)
#ifndef NDEBUG
    , m_didDispatchDidCommitLoad(false)
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
    setPolicyDocumentLoader(m_client->createDocumentLoader(ResourceRequest(KURL(ParsedURLString, "")), SubstituteData()).get());
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

    if (!defers) {
        m_frame->redirectScheduler()->startTimer();
        startCheckCompleteTimer();
    }
}

Frame* FrameLoader::createWindow(FrameLoader* frameLoaderForFrameLookup, const FrameLoadRequest& request, const WindowFeatures& features, bool& created)
{ 
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank") {
        Frame* frame = frameLoaderForFrameLookup->frame()->tree()->find(request.frameName());
        if (frame && shouldAllowNavigation(frame)) {
            if (!request.resourceRequest().url().isEmpty())
                frame->loader()->loadFrameRequest(request, false, false, 0, 0);
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

void FrameLoader::changeLocation(const KURL& url, const String& referrer, bool lockHistory, bool lockBackForwardList, bool userGesture, bool refresh)
{
    RefPtr<Frame> protect(m_frame);

    ResourceRequest request(url, referrer, refresh ? ReloadIgnoringCacheData : UseProtocolCachePolicy);
    
    if (m_frame->script()->executeIfJavaScriptURL(request.url(), userGesture))
        return;

    urlSelected(request, "_self", 0, lockHistory, lockBackForwardList, userGesture);
}

void FrameLoader::urlSelected(const ResourceRequest& request, const String& passedTarget, PassRefPtr<Event> triggeringEvent, bool lockHistory, bool lockBackForwardList, bool userGesture)
{
    if (m_frame->script()->executeIfJavaScriptURL(request.url(), userGesture, false))
        return;

    String target = passedTarget;
    if (target.isEmpty())
        target = m_frame->document()->baseTarget();

    FrameLoadRequest frameRequest(request, target);

    if (frameRequest.resourceRequest().httpReferrer().isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);
    addHTTPOriginIfNeeded(frameRequest.resourceRequest(), outgoingOrigin());

    loadFrameRequest(frameRequest, lockHistory, lockBackForwardList, triggeringEvent, 0);
}

bool FrameLoader::requestFrame(HTMLFrameOwnerElement* ownerElement, const String& urlString, const AtomicString& frameName)
{
    // Support for <frame src="javascript:string">
    KURL scriptURL;
    KURL url;
    if (protocolIsJavaScript(urlString)) {
        scriptURL = completeURL(urlString); // completeURL() encodes the URL.
        url = blankURL();
    } else
        url = completeURL(urlString);

    Frame* frame = ownerElement->contentFrame();
    if (frame)
        frame->redirectScheduler()->scheduleLocationChange(url.string(), m_outgoingReferrer, true, true, isProcessingUserGesture());
    else
        frame = loadSubframe(ownerElement, url, frameName, m_outgoingReferrer);
    
    if (!frame)
        return false;

    if (!scriptURL.isEmpty())
        frame->script()->executeIfJavaScriptURL(scriptURL);

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

    if (!SecurityOrigin::canLoad(url, referrer, 0)) {
        FrameLoader::reportLocalLoadFailed(m_frame, url.string());
        return 0;
    }

    bool hideReferrer = SecurityOrigin::shouldHideReferrer(url, referrer);
    RefPtr<Frame> frame = m_client->createFrame(url, name, ownerElement, hideReferrer ? String() : referrer, allowsScrolling, marginWidth, marginHeight);

    if (!frame)  {
        checkCallImplicitClose();
        return 0;
    }
    
    frame->loader()->m_isComplete = false;
   
    RenderObject* renderer = ownerElement->renderer();
    FrameView* view = frame->view();
    if (renderer && renderer->isWidget() && view)
        toRenderWidget(renderer)->setWidget(view);
    
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

void FrameLoader::submitForm(const char* action, const String& url, PassRefPtr<FormData> formData,
    const String& target, const String& contentType, const String& boundary,
    bool lockHistory, PassRefPtr<Event> event, PassRefPtr<FormState> formState)
{
    ASSERT(action);
    ASSERT(strcmp(action, "GET") == 0 || strcmp(action, "POST") == 0);
    ASSERT(formData);
    ASSERT(formState);
    ASSERT(formState->sourceFrame() == m_frame);
    
    if (!m_frame->page())
        return;
    
    KURL u = completeURL(url.isNull() ? "" : url);
    if (u.isEmpty())
        return;

    if (protocolIsJavaScript(u)) {
        m_isExecutingJavaScriptFormAction = true;
        m_frame->script()->executeIfJavaScriptURL(u, false, false);
        m_isExecutingJavaScriptFormAction = false;
        return;
    }

    FrameLoadRequest frameRequest;

    String targetOrBaseTarget = target.isEmpty() ? m_frame->document()->baseTarget() : target;
    Frame* targetFrame = findFrameForNavigation(targetOrBaseTarget);
    if (!targetFrame) {
        targetFrame = m_frame;
        frameRequest.setFrameName(targetOrBaseTarget);
    }
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

    if (m_frame->tree()->isDescendantOf(targetFrame)) {
        if (m_submittedFormURL == u)
            return;
        m_submittedFormURL = u;
    }

    formData->generateFiles(m_frame->page()->chrome()->client());
    
    if (!m_outgoingReferrer.isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_outgoingReferrer);

    if (strcmp(action, "GET") == 0)
        u.setQuery(formData->flattenToString());
    else {
        frameRequest.resourceRequest().setHTTPMethod("POST");
        frameRequest.resourceRequest().setHTTPBody(formData);

        // construct some user headers if necessary
        if (contentType.isNull() || contentType == "application/x-www-form-urlencoded")
            frameRequest.resourceRequest().setHTTPContentType(contentType);
        else // contentType must be "multipart/form-data"
            frameRequest.resourceRequest().setHTTPContentType(contentType + "; boundary=" + boundary);
    }

    frameRequest.resourceRequest().setURL(u);
    addHTTPOriginIfNeeded(frameRequest.resourceRequest(), outgoingOrigin());

    targetFrame->redirectScheduler()->scheduleFormSubmission(frameRequest, lockHistory, event, formState);
}

void FrameLoader::stopLoading(UnloadEventPolicy unloadEventPolicy, DatabasePolicy databasePolicy)
{
    if (m_frame->document() && m_frame->document()->tokenizer())
        m_frame->document()->tokenizer()->stopParsing();

    if (unloadEventPolicy != UnloadEventPolicyNone) {
        if (m_frame->document()) {
            if (m_didCallImplicitClose && !m_wasUnloadEventEmitted) {
                Node* currentFocusedNode = m_frame->document()->focusedNode();
                if (currentFocusedNode)
                    currentFocusedNode->aboutToUnload();
                m_unloadEventBeingDispatched = true;
                if (m_frame->domWindow()) {
                    if (unloadEventPolicy == UnloadEventPolicyUnloadAndPageHide)
                        m_frame->domWindow()->dispatchEvent(PageTransitionEvent::create(EventNames().pagehideEvent, m_frame->document()->inPageCache()), m_frame->document());
                    if (!m_frame->document()->inPageCache())
                        m_frame->domWindow()->dispatchEvent(Event::create(eventNames().unloadEvent, false, false), m_frame->domWindow()->document());
                }
                m_unloadEventBeingDispatched = false;
                if (m_frame->document())
                    m_frame->document()->updateStyleIfNeeded();
                m_wasUnloadEventEmitted = true;
            }
        }

        // Dispatching the unload event could have made m_frame->document() null.
        if (m_frame->document() && !m_frame->document()->inPageCache())
            m_frame->document()->removeAllEventListeners();
    }

    m_isComplete = true; // to avoid calling completed() in finishedParsing()
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
        if (databasePolicy == DatabasePolicyStop)
            doc->stopDatabases();
#endif
    }

    // tell all subframes to stop as well
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopLoading(unloadEventPolicy);

    m_frame->redirectScheduler()->cancel();
}

void FrameLoader::stop()
{
    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it will be deleted by checkCompleted().
    RefPtr<Frame> protector(m_frame);
    
    if (m_frame->document()->tokenizer())
        m_frame->document()->tokenizer()->stopParsing();
    m_frame->document()->finishParsing();

    if (m_iconLoader)
        m_iconLoader->stopLoading();
}

bool FrameLoader::closeURL()
{
    history()->saveDocumentState();
    
    // Should only send the pagehide event here if the current document exists and has not been placed in the page cache.    
    Document* currentDocument = m_frame->document();
    stopLoading(currentDocument && !currentDocument->inPageCache() ? UnloadEventPolicyUnloadAndPageHide : UnloadEventPolicyUnloadOnly);
    
    m_frame->editor()->clearUndoRedoOperations();
    return true;
}

KURL FrameLoader::iconURL()
{
    // If this isn't a top level frame, return nothing
    if (m_frame->tree() && m_frame->tree()->parent())
        return KURL();

    // If we have an iconURL from a Link element, return that
    if (!m_frame->document()->iconURL().isEmpty())
        return KURL(ParsedURLString, m_frame->document()->iconURL());

    // Don't return a favicon iconURL unless we're http or https
    if (!m_URL.protocolInHTTPFamily())
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
    if (m_frame->redirectScheduler()->redirectScheduledDuringLoad()) {
        // A redirect was scheduled before the document was created.
        // This can happen when one frame changes another frame's location.
        return false;
    }

    m_frame->redirectScheduler()->cancel();
    m_frame->editor()->clearLastEditCommand();
    closeURL();

    m_isComplete = false;
    m_isLoadingMainResource = true;
    m_didCallImplicitClose = false;

    // If we are still in the process of initializing an empty document then
    // its frame is not in a consistent state for rendering, so avoid setJSStatusBarText
    // since it may cause clients to attempt to render the frame.
    if (!m_creatingInitialEmptyDocument) {
        m_frame->setJSStatusBarText(String());
        m_frame->setJSDefaultStatusBarText(String());
    }
    m_URL = url;
    if (m_URL.protocolInHTTPFamily() && !m_URL.host().isEmpty() && m_URL.path().isEmpty())
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
    m_frame->redirectScheduler()->cancel(); 
    if (m_frame->document()->url() != blankURL())
        m_URL = m_frame->document()->url();
}


void FrameLoader::cancelAndClear()
{
    m_frame->redirectScheduler()->cancel();

    if (!m_isComplete)
        closeURL();

    clear(false);
    m_frame->script()->updatePlatformScriptObjects();
}

void FrameLoader::replaceDocument(const String& html)
{
    stopAllLoaders();
    begin(m_URL, true, m_frame->document()->securityOrigin());
    write(html);
    end();
}

void FrameLoader::clear(bool clearWindowProperties, bool clearScriptObjects, bool clearFrameView)
{
    m_frame->editor()->clear();

    if (!m_needsClear)
        return;
    m_needsClear = false;
    
    if (!m_frame->document()->inPageCache()) {
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
    if (clearFrameView && m_frame->view())
        m_frame->view()->clear();

    m_frame->setSelectionGranularity(CharacterGranularity);

    // Do not drop the document before the ScriptController and view are cleared
    // as some destructors might still try to access the document.
    m_frame->setDocument(0);
    m_decoder = 0;

    m_containsPlugIns = false;

    if (clearScriptObjects)
        m_frame->script()->clearScriptObjects();

    m_frame->redirectScheduler()->clear();

    m_checkTimer.stop();
    m_shouldCallCheckCompleted = false;
    m_shouldCallCheckLoadComplete = false;

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
    
    if (m_documentLoader) {
        String ptitle = m_documentLoader->title();
        // If we have a title let the WebView know about it.
        if (!ptitle.isNull())
            m_client->dispatchDidReceiveTitle(ptitle);
    }

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

    m_frame->redirectScheduler()->scheduleRedirect(delay, url);
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

    RefPtr<Document> document;

    // Create a new document before clearing the frame, because it may need to inherit an aliased security context.
    if (!m_isDisplayingInitialEmptyDocument && m_client->shouldUsePluginDocument(m_responseMIMEType))
        document = PluginDocument::create(m_frame);
    else if (!m_client->hasHTMLView())
        document = PlaceholderDocument::create(m_frame);
    else
        document = DOMImplementation::createDocument(m_responseMIMEType, m_frame, m_frame->inViewSourceMode());

    bool resetScripting = !(m_isDisplayingInitialEmptyDocument && m_frame->document()->securityOrigin()->isSecureTransitionTo(url));
    clear(resetScripting, resetScripting);
    if (resetScripting)
        m_frame->script()->updatePlatformScriptObjects();

    m_needsClear = true;
    m_isComplete = false;
    m_didCallImplicitClose = false;
    m_isLoadingMainResource = true;
    m_isDisplayingInitialEmptyDocument = m_creatingInitialEmptyDocument;

    KURL ref(url);
    ref.setUser(String());
    ref.setPass(String());
    ref.removeFragmentIdentifier();
    m_outgoingReferrer = ref.string();
    m_URL = url;

    document->setURL(m_URL);
    m_frame->setDocument(document);

    if (m_decoder)
        document->setDecoder(m_decoder.get());
    if (forcedSecurityOrigin)
        document->setSecurityOrigin(forcedSecurityOrigin.get());

    m_frame->domWindow()->setURL(document->url());
    m_frame->domWindow()->setSecurityOrigin(document->securityOrigin());

    if (dispatch)
        dispatchWindowObjectAvailable();
    
    updateFirstPartyForCookies();

    Settings* settings = document->settings();
    document->docLoader()->setAutoLoadImages(settings && settings->loadsImagesAutomatically());

    if (m_documentLoader) {
        String dnsPrefetchControl = m_documentLoader->response().httpHeaderField("X-DNS-Prefetch-Control");
        if (!dnsPrefetchControl.isEmpty())
            document->parseDNSPrefetchControlHeader(dnsPrefetchControl);
    }

    history()->restoreDocumentState();

    document->implicitOpen();
    
    if (m_frame->view() && m_client->hasHTMLView())
        m_frame->view()->setContentsSize(IntSize());
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
        if (Settings* settings = m_frame->settings()) {
            m_decoder = TextResourceDecoder::create(m_responseMIMEType,
                settings->defaultTextEncodingName(),
                settings->usesEncodingDetector());
            Frame* parentFrame = m_frame->tree()->parent();
            // Set the hint encoding to the parent frame encoding only if
            // the parent and the current frames share the security origin.
            // We impose this condition because somebody can make a child frame 
            // containing a carefully crafted html/javascript in one encoding
            // that can be mistaken for hintEncoding (or related encoding) by
            // an auto detector. When interpreted in the latter, it could be
            // an attack vector.
            // FIXME: This might be too cautious for non-7bit-encodings and
            // we may consider relaxing this later after testing.
            if (canReferToParentFrameEncoding(m_frame, parentFrame))
                m_decoder->setHintEncoding(parentFrame->document()->decoder());
        } else
            m_decoder = TextResourceDecoder::create(m_responseMIMEType, String());
        Frame* parentFrame = m_frame->tree()->parent();
        if (m_encoding.isEmpty()) {
            if (canReferToParentFrameEncoding(m_frame, parentFrame))
                m_decoder->setEncoding(parentFrame->document()->inputEncoding(), TextResourceDecoder::EncodingFromParentFrame);
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
    if (m_isLoadingMainResource || !m_frame->page() || !m_frame->document())
        return;

    // http://bugs.webkit.org/show_bug.cgi?id=10854
    // The frame's last ref may be removed and it can be deleted by checkCompleted(), 
    // so we'll add a protective refcount
    RefPtr<Frame> protector(m_frame);

    // make sure nothing's left in there
    write(0, 0, true);
    m_frame->document()->finishParsing();
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

    // People who want to avoid loading images generally want to avoid loading all images.
    // Now that we've accounted for URL mapping, avoid starting the network load if images aren't set to display automatically.
    Settings* settings = m_frame->settings();
    if (settings && !settings->loadsImagesAutomatically())
        return;

    // This is either a reload or the icon database said "yes, load the icon", so kick off the load!
    if (!m_iconLoader)
        m_iconLoader.set(IconLoader::create(m_frame).release());
        
    m_iconLoader->startLoading();
}

void FrameLoader::commitIconURLToIconDatabase(const KURL& icon)
{
    ASSERT(iconDatabase());
    LOG(IconDatabase, "Committing iconURL %s to database for pageURLs %s and %s", icon.string().ascii().data(), m_URL.string().ascii().data(), originalRequestURL().string().ascii().data());
    iconDatabase()->setIconURLForPageURL(icon.string(), m_URL.string());
    iconDatabase()->setIconURLForPageURL(icon.string(), originalRequestURL().string());
}

void HistoryController::restoreDocumentState()
{
    Document* doc = m_frame->document();
        
    HistoryItem* itemToRestore = 0;
    
    switch (m_frame->loader()->loadType()) {
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadFromOrigin:
        case FrameLoadTypeSame:
        case FrameLoadTypeReplace:
            break;
        case FrameLoadTypeBack:
        case FrameLoadTypeBackWMLDeckNotAccessible:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
        case FrameLoadTypeRedirectWithLockedBackForwardList:
        case FrameLoadTypeStandard:
            itemToRestore = m_currentItem.get(); 
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
    if (!m_URL.hasFragmentIdentifier() && !m_frame->document()->cssTarget())
        return;

    String fragmentIdentifier = m_URL.fragmentIdentifier();
    if (gotoAnchor(fragmentIdentifier))
        return;

    // Try again after decoding the ref, based on the document's encoding.
    if (m_decoder)
        gotoAnchor(decodeURLEscapeSequences(fragmentIdentifier, m_decoder->encoding()));
}

void FrameLoader::finishedParsing()
{
    if (m_creatingInitialEmptyDocument)
        return;

    m_frame->injectUserScripts(InjectAtDocumentEnd);

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
    checkCompleted();
}

bool FrameLoader::allChildrenAreComplete() const
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!child->loader()->m_isComplete)
            return false;
    }
    return true;
}

bool FrameLoader::allAncestorsAreComplete() const
{
    for (Frame* ancestor = m_frame; ancestor; ancestor = ancestor->tree()->parent()) {
        if (!ancestor->loader()->m_isComplete)
            return false;
    }
    return true;
}

void FrameLoader::checkCompleted()
{
    m_shouldCallCheckCompleted = false;

    if (m_frame->view())
        m_frame->view()->checkStopDelayingDeferredRepaints();

    // Any frame that hasn't completed yet?
    if (!allChildrenAreComplete())
        return;

    // Have we completed before?
    if (m_isComplete)
        return;

    // Are we still parsing?
    if (m_frame->document()->parsing())
        return;

    // Still waiting for images/scripts?
    if (numRequests(m_frame->document()))
        return;

    // OK, completed.
    m_isComplete = true;

    RefPtr<Frame> protect(m_frame);
    checkCallImplicitClose(); // if we didn't do it before

    m_frame->redirectScheduler()->startTimer();

    completed();
    if (m_frame->page())
        checkLoadComplete();
}

void FrameLoader::checkTimerFired(Timer<FrameLoader>*)
{
    if (Page* page = m_frame->page()) {
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
    if (m_didCallImplicitClose || m_frame->document()->parsing())
        return;

    if (!allChildrenAreComplete())
        return; // still got a frame running -> too early

    m_didCallImplicitClose = true;
    m_wasUnloadEventEmitted = false;
    m_frame->document()->implicitClose();
}

KURL FrameLoader::baseURL() const
{
    ASSERT(m_frame->document());
    return m_frame->document()->baseURL();
}

KURL FrameLoader::completeURL(const String& url)
{
    ASSERT(m_frame->document());
    return m_frame->document()->completeURL(url);
}

void HistoryController::setCurrentItem(HistoryItem* item)
{
    m_currentItem = item;
}

void HistoryController::setProvisionalItem(HistoryItem* item)
{
    m_provisionalItem = item;
}

void FrameLoader::loadURLIntoChildFrame(const KURL& url, const String& referer, Frame* childFrame)
{
    ASSERT(childFrame);

    HistoryItem* parentItem = history()->currentItem();
    FrameLoadType loadType = this->loadType();
    FrameLoadType childLoadType = FrameLoadTypeRedirectWithLockedBackForwardList;

    KURL workingURL = url;
    
    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    if (parentItem && parentItem->children().size() && isBackForwardLoadType(loadType)) {
        HistoryItem* childItem = parentItem->childItemWithTarget(childFrame->tree()->name());
        if (childItem) {
            // Use the original URL to ensure we get all the side-effects, such as
            // onLoad handlers, of any redirects that happened. An example of where
            // this is needed is Radar 3213556.
            workingURL = KURL(ParsedURLString, childItem->originalURLString());
            childLoadType = loadType;
            childFrame->loader()->history()->setProvisionalItem(childItem);
        }
    }

    RefPtr<Archive> subframeArchive = activeDocumentLoader()->popArchiveForSubframe(childFrame->tree()->name());
    
    if (subframeArchive)
        childFrame->loader()->loadArchive(subframeArchive.release());
    else
        childFrame->loader()->loadURL(workingURL, referer, String(), false, childLoadType, 0, 0);
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

    if (FrameView* view = m_frame->view())
        view->maintainScrollPositionAtAnchor(anchorNode ? static_cast<Node*>(anchorNode) : m_frame->document());

    return true;
}

bool FrameLoader::requestObject(RenderPart* renderer, const String& url, const AtomicString& frameName,
    const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    if (url.isEmpty() && mimeType.isEmpty())
        return false;
    
    if (!m_frame->script()->xssAuditor()->canLoadObject(url)) {
        // It is unsafe to honor the request for this object.
        return false;
    }

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

static HTMLPlugInElement* toPlugInElement(Node* node)
{
    if (!node)
        return 0;

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    ASSERT(node->hasTagName(objectTag) || node->hasTagName(embedTag) 
        || node->hasTagName(videoTag) || node->hasTagName(audioTag)
        || node->hasTagName(appletTag));
#else
    ASSERT(node->hasTagName(objectTag) || node->hasTagName(embedTag) 
        || node->hasTagName(appletTag));
#endif

    return static_cast<HTMLPlugInElement*>(node);
}
    
bool FrameLoader::loadPlugin(RenderPart* renderer, const KURL& url, const String& mimeType, 
    const Vector<String>& paramNames, const Vector<String>& paramValues, bool useFallback)
{
    RefPtr<Widget> widget;

    if (renderer && !useFallback) {
        HTMLPlugInElement* element = toPlugInElement(renderer->node());

        if (!SecurityOrigin::canLoad(url, String(), frame()->document())) {
            FrameLoader::reportLocalLoadFailed(m_frame, url.string());
            return false;
        }

        widget = m_client->createPlugin(IntSize(renderer->contentWidth(), renderer->contentHeight()),
                                        element, url, paramNames, paramValues, mimeType,
                                        m_frame->document()->isPluginDocument() && !m_containsPlugIns);
        if (widget) {
            renderer->setWidget(widget);
            m_containsPlugIns = true;
        }
    }

    return widget != 0;
}

String FrameLoader::outgoingReferrer() const
{
    return m_outgoingReferrer;
}

String FrameLoader::outgoingOrigin() const
{
    return m_frame->document()->securityOrigin()->toString();
}

bool FrameLoader::isMixedContent(SecurityOrigin* context, const KURL& url)
{
    if (context->protocol() != "https")
        return false;  // We only care about HTTPS security origins.

    if (url.protocolIs("https") || url.protocolIs("about") || url.protocolIs("data"))
        return false;  // Loading these protocols is secure.

    return true;
}

void FrameLoader::checkIfDisplayInsecureContent(SecurityOrigin* context, const KURL& url)
{
    if (!isMixedContent(context, url))
        return;

    m_client->didDisplayInsecureContent();
}

void FrameLoader::checkIfRunInsecureContent(SecurityOrigin* context, const KURL& url)
{
    if (!isMixedContent(context, url))
        return;

    m_client->didRunInsecureContent(context);
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

void FrameLoader::handleFallbackContent()
{
    HTMLFrameOwnerElement* owner = m_frame->ownerElement();
    if (!owner || !owner->hasTagName(objectTag))
        return;
    static_cast<HTMLObjectElement*>(owner)->renderFallbackContent();
}

void FrameLoader::provisionalLoadStarted()
{    
    m_firstLayoutDone = false;
    m_frame->redirectScheduler()->cancel(true);
    m_client->provisionalLoadStarted();
}

bool FrameLoader::isProcessingUserGesture()
{
    Frame* frame = m_frame->tree()->top();
    if (!frame->script()->isEnabled())
        return true; // If JavaScript is disabled, a user gesture must have initiated the navigation.
    return frame->script()->processingUserGesture(); // FIXME: Use pageIsProcessingUserGesture.
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

#if ENABLE(WML)
static inline bool frameContainsWMLContent(Frame* frame)
{
    Document* document = frame ? frame->document() : 0;
    if (!document)
        return false;

    return document->containsWMLContent() || document->isWMLDocument();
}
#endif

bool FrameLoader::canCachePageContainingThisFrame()
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!child->loader()->canCachePageContainingThisFrame())
            return false;
    }
            
    return m_documentLoader
        && m_documentLoader->mainDocumentError().isNull()
        // FIXME: If we ever change this so that frames with plug-ins will be cached,
        // we need to make sure that we don't cache frames that have outstanding NPObjects
        // (objects created by the plug-in). Since there is no way to pause/resume a Netscape plug-in,
        // they would need to be destroyed and then recreated, and there is no way that we can recreate
        // the right NPObjects. See <rdar://problem/5197041> for more information.
        && !m_containsPlugIns
        && !m_URL.protocolIs("https")
#ifndef PAGE_CACHE_ACCEPTS_UNLOAD_HANDLERS
        && (!m_frame->domWindow() || !m_frame->domWindow()->hasEventListeners(eventNames().unloadEvent))
#endif
#if ENABLE(DATABASE)
        && !m_frame->document()->hasOpenDatabases()
#endif
#if ENABLE(SHARED_WORKERS)
        && !SharedWorkerRepository::hasSharedWorkers(m_frame->document())
#endif
        && !m_frame->document()->usingGeolocation()
        && history()->currentItem()
        && !m_quickRedirectComing
        && !m_documentLoader->isLoadingInAPISense()
        && !m_documentLoader->isStopping()
        && m_frame->document()->canSuspendActiveDOMObjects()
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        // FIXME: We should investigating caching frames that have an associated
        // application cache. <rdar://problem/5917899> tracks that work.
        && m_documentLoader->applicationCacheHost()->canCacheInPageCache()
#endif
#if ENABLE(WML)
        && !frameContainsWMLContent(m_frame)
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
        if (m_containsPlugIns)
            { PCLOG("   -Frame contains plugins"); cannotCache = true; }
        if (m_URL.protocolIs("https"))
            { PCLOG("   -Frame is HTTPS"); cannotCache = true; }
#ifndef PAGE_CACHE_ACCEPTS_UNLOAD_HANDLERS
        if (m_frame->domWindow() && m_frame->domWindow()->hasEventListeners(eventNames().unloadEvent))
            { PCLOG("   -Frame has an unload event listener"); cannotCache = true; }
#endif
#if ENABLE(DATABASE)
        if (m_frame->document()->hasOpenDatabases())
            { PCLOG("   -Frame has open database handles"); cannotCache = true; }
#endif
#if ENABLE(SHARED_WORKERS)
        if (SharedWorkerRepository::hasSharedWorkers(m_frame->document()))
            { PCLOG("   -Frame has associated SharedWorkers"); cannotCache = true; }
#endif
        if (m_frame->document()->usingGeolocation())
            { PCLOG("   -Frame uses Geolocation"); cannotCache = true; }
        if (!history()->currentItem())
            { PCLOG("   -No current history item"); cannotCache = true; }
        if (m_quickRedirectComing)
            { PCLOG("   -Quick redirect is coming"); cannotCache = true; }
        if (m_documentLoader->isLoadingInAPISense())
            { PCLOG("   -DocumentLoader is still loading in API sense"); cannotCache = true; }
        if (m_documentLoader->isStopping())
            { PCLOG("   -DocumentLoader is in the middle of stopping"); cannotCache = true; }
        if (!m_frame->document()->canSuspendActiveDOMObjects())
            { PCLOG("   -The document cannot suspect its active DOM Objects"); cannotCache = true; }
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        if (!m_documentLoader->applicationCacheHost()->canCacheInPageCache())
            { PCLOG("   -The DocumentLoader uses an application cache"); cannotCache = true; }
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

void FrameLoader::updateFirstPartyForCookies()
{
    if (m_frame->tree()->parent())
        setFirstPartyForCookies(m_frame->tree()->parent()->document()->firstPartyForCookies());
    else
        setFirstPartyForCookies(m_URL);
}

void FrameLoader::setFirstPartyForCookies(const KURL& url)
{
    m_frame->document()->setFirstPartyForCookies(url);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->setFirstPartyForCookies(url);
}

class HashChangeEventTask : public ScriptExecutionContext::Task {
public:
    static PassRefPtr<HashChangeEventTask> create(PassRefPtr<Document> document)
    {
        return adoptRef(new HashChangeEventTask(document));
    }
    
    virtual void performTask(ScriptExecutionContext* context)
    {
        ASSERT_UNUSED(context, context->isDocument());
        m_document->dispatchWindowEvent(Event::create(eventNames().hashchangeEvent, false, false));
    }
    
private:
    HashChangeEventTask(PassRefPtr<Document> document)
        : m_document(document)
    {
        ASSERT(m_document);
    }
    
    RefPtr<Document> m_document;
};

// This does the same kind of work that didOpenURL does, except it relies on the fact
// that a higher level already checked that the URLs match and the scrolling is the right thing to do.
void FrameLoader::scrollToAnchor(const KURL& url)
{
    ASSERT(equalIgnoringFragmentIdentifier(url, m_URL));
    if (equalIgnoringFragmentIdentifier(url, m_URL) && !equalIgnoringNullity(url.fragmentIdentifier(), m_URL.fragmentIdentifier())) {
        Document* currentDocument = frame()->document();
        currentDocument->postTask(HashChangeEventTask::create(currentDocument));
    }
    
    m_URL = url;
    history()->updateForAnchorScroll();

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

void FrameLoader::completed()
{
    RefPtr<Frame> protect(m_frame);
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->redirectScheduler()->startTimer();
    if (Frame* parent = m_frame->tree()->parent())
        parent->loader()->checkCompleted();
    if (m_frame->view())
        m_frame->view()->maintainScrollPositionAtAnchor(0);
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

// This is a hack to allow keep navigation to http/https feeds working. To remove this
// we need to introduce new API akin to registerURLSchemeAsLocal, that registers a
// protocols navigation policy.
static bool isFeedWithNestedProtocolInHTTPFamily(const KURL& url)
{
    const String& urlString = url.string();
    if (!urlString.startsWith("feed", false))
        return false;

    return urlString.startsWith("feed://", false) 
        || urlString.startsWith("feed:http:", false) || urlString.startsWith("feed:https:", false)
        || urlString.startsWith("feeds:http:", false) || urlString.startsWith("feeds:https:", false)
        || urlString.startsWith("feedsearch:http:", false) || urlString.startsWith("feedsearch:https:", false);
}

void FrameLoader::loadFrameRequest(const FrameLoadRequest& request, bool lockHistory, bool lockBackForwardList,
    PassRefPtr<Event> event, PassRefPtr<FormState> formState)
{    
    KURL url = request.resourceRequest().url();

    String referrer;
    String argsReferrer = request.resourceRequest().httpReferrer();
    if (!argsReferrer.isEmpty())
        referrer = argsReferrer;
    else
        referrer = m_outgoingReferrer;

    ASSERT(frame()->document());
    if (SecurityOrigin::shouldTreatURLAsLocal(url.string()) && !isFeedWithNestedProtocolInHTTPFamily(url)) {
        if (!SecurityOrigin::canLoad(url, String(), frame()->document()) && !SecurityOrigin::canLoad(url, referrer, 0)) {
            FrameLoader::reportLocalLoadFailed(m_frame, url.string());
            return;
        }
    }

    if (SecurityOrigin::shouldHideReferrer(url, referrer))
        referrer = String();
    
    FrameLoadType loadType;
    if (request.resourceRequest().cachePolicy() == ReloadIgnoringCacheData)
        loadType = FrameLoadTypeReload;
    else if (lockBackForwardList)
        loadType = FrameLoadTypeRedirectWithLockedBackForwardList;
    else
        loadType = FrameLoadTypeStandard;

    if (request.resourceRequest().httpMethod() == "POST")
        loadPostRequest(request.resourceRequest(), referrer, request.frameName(), lockHistory, loadType, event, formState.get());
    else
        loadURL(request.resourceRequest().url(), referrer, request.frameName(), lockHistory, loadType, event, formState.get());

    // FIXME: It's possible this targetFrame will not be the same frame that was targeted by the actual
    // load if frame names have changed.
    Frame* sourceFrame = formState ? formState->sourceFrame() : m_frame;
    Frame* targetFrame = sourceFrame->loader()->findFrameForNavigation(request.frameName());
    if (targetFrame && targetFrame != sourceFrame) {
        if (Page* page = targetFrame->page())
            page->chrome()->focus();
    }
}

void FrameLoader::loadURL(const KURL& newURL, const String& referrer, const String& frameName, bool lockHistory, FrameLoadType newLoadType,
    PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
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

    // The search for a target frame is done earlier in the case of form submission.
    Frame* targetFrame = isFormSubmission ? 0 : findFrameForNavigation(frameName);
    if (targetFrame && targetFrame != m_frame) {
        targetFrame->loader()->loadURL(newURL, referrer, String(), lockHistory, newLoadType, event, formState.release());
        return;
    }

    if (m_unloadEventBeingDispatched)
        return;

    NavigationAction action(newURL, newLoadType, isFormSubmission, event);

    if (!targetFrame && !frameName.isEmpty()) {
        policyChecker()->checkNewWindowPolicy(action, FrameLoader::callContinueLoadAfterNewWindowPolicy,
            request, formState.release(), frameName, this);
        return;
    }

    RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;

    bool sameURL = shouldTreatURLAsSameAsCurrent(newURL);
    
    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (shouldScrollToAnchor(isFormSubmission, newLoadType, newURL)) {
        oldDocumentLoader->setTriggeringAction(action);
        policyChecker()->stopCheck();
        policyChecker()->setLoadType(newLoadType);
        policyChecker()->checkNavigationPolicy(request, oldDocumentLoader.get(), formState.release(),
            callContinueFragmentScrollAfterNavigationPolicy, this);
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        bool isRedirect = m_quickRedirectComing;
        loadWithNavigationAction(request, action, lockHistory, newLoadType, formState.release());
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

void FrameLoader::load(const ResourceRequest& request, bool lockHistory)
{
    load(request, SubstituteData(), lockHistory);
}

void FrameLoader::load(const ResourceRequest& request, const SubstituteData& substituteData, bool lockHistory)
{
    if (m_inStopAllLoaders)
        return;
        
    // FIXME: is this the right place to reset loadType? Perhaps this should be done after loading is finished or aborted.
    m_loadType = FrameLoadTypeStandard;
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, substituteData);
    if (lockHistory && m_documentLoader)
        loader->setClientRedirectSourceForHistory(m_documentLoader->didCreateGlobalHistoryEntry() ? m_documentLoader->urlForHistory() : m_documentLoader->clientRedirectSourceForHistory());
    load(loader.get());
}

void FrameLoader::load(const ResourceRequest& request, const String& frameName, bool lockHistory)
{
    if (frameName.isEmpty()) {
        load(request, lockHistory);
        return;
    }

    Frame* frame = findFrameForNavigation(frameName);
    if (frame) {
        frame->loader()->load(request, lockHistory);
        return;
    }

    policyChecker()->checkNewWindowPolicy(NavigationAction(request.url(), NavigationTypeOther), FrameLoader::callContinueLoadAfterNewWindowPolicy, request, 0, frameName, this);
}

void FrameLoader::loadWithNavigationAction(const ResourceRequest& request, const NavigationAction& action, bool lockHistory, FrameLoadType type, PassRefPtr<FormState> formState)
{
    RefPtr<DocumentLoader> loader = m_client->createDocumentLoader(request, SubstituteData());
    if (lockHistory && m_documentLoader)
        loader->setClientRedirectSourceForHistory(m_documentLoader->didCreateGlobalHistoryEntry() ? m_documentLoader->urlForHistory() : m_documentLoader->clientRedirectSourceForHistory());

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

    if (m_unloadEventBeingDispatched)
        return;

    policyChecker()->setLoadType(type);
    RefPtr<FormState> formState = prpFormState;
    bool isFormSubmission = formState;

    const KURL& newURL = loader->request().url();

    if (shouldScrollToAnchor(isFormSubmission, policyChecker()->loadType(), newURL)) {
        RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;
        NavigationAction action(newURL, policyChecker()->loadType(), isFormSubmission);

        oldDocumentLoader->setTriggeringAction(action);
        policyChecker()->stopCheck();
        policyChecker()->checkNavigationPolicy(loader->request(), oldDocumentLoader.get(), formState,
            callContinueFragmentScrollAfterNavigationPolicy, this);
    } else {
        if (Frame* parent = m_frame->tree()->parent())
            loader->setOverrideEncoding(parent->loader()->documentLoader()->overrideEncoding());

        policyChecker()->stopCheck();
        setPolicyDocumentLoader(loader);
        if (loader->triggeringAction().isEmpty())
            loader->setTriggeringAction(NavigationAction(newURL, policyChecker()->loadType(), isFormSubmission));

        policyChecker()->checkNavigationPolicy(loader->request(), loader, formState,
            callContinueLoadAfterNavigationPolicy, this);
    }
}

void FrameLoader::reportLocalLoadFailed(Frame* frame, const String& url)
{
    ASSERT(!url.isEmpty());
    if (!frame)
        return;

    frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, "Not allowed to load local resource: " + url, 0, String());
}

const ResourceRequest& FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->originalRequest();
}

void FrameLoader::receivedData(const char* data, int length)
{
    activeDocumentLoader()->receivedData(data, length);
}

bool FrameLoader::willLoadMediaElementURL(KURL& url)
{
    ResourceRequest request(url);

    unsigned long identifier;
    ResourceError error;
    requestFromDelegate(request, identifier, error);
    sendRemainingDelegateMessages(identifier, ResourceResponse(url, String(), -1, String(), String()), -1, error);

    url = request.url();

    return error.isNull();
}

ResourceError FrameLoader::interruptionForPolicyChangeError(const ResourceRequest& request)
{
    return m_client->interruptForPolicyChangeError(request);
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(DocumentLoader* docLoader)
{
    KURL unreachableURL = docLoader->unreachableURL();

    if (unreachableURL.isEmpty())
        return false;

    if (!isBackForwardLoadType(policyChecker()->loadType()))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    DocumentLoader* compareDocumentLoader = 0;
    if (policyChecker()->delegateIsDecidingNavigationPolicy() || policyChecker()->delegateIsHandlingUnimplementablePolicy())
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
    //     navigate the target frame's opener per above or it is the opener of
    //     the target frame.

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

    // Let a frame navigate its opener if the opener is a top-level window.
    if (!targetFrame->tree()->parent() && m_frame->loader()->opener() == targetFrame)
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
        targetFrame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, message, 1, String());
    }
    
    return false;
}

void FrameLoader::stopLoadingSubframes()
{
    for (RefPtr<Frame> child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->stopAllLoaders();
}

void FrameLoader::stopAllLoaders(DatabasePolicy databasePolicy)
{
    ASSERT(!m_frame->document() || !m_frame->document()->inPageCache());
    if (m_unloadEventBeingDispatched)
        return;

    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_inStopAllLoaders)
        return;

    m_inStopAllLoaders = true;

    policyChecker()->stopCheck();

    stopLoadingSubframes();
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading(databasePolicy);
    if (m_documentLoader)
        m_documentLoader->stopLoading(databasePolicy);

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

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
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

    LOG(PageCache, "WebCoreLoading %s: About to commit provisional load from previous URL '%s' to new URL '%s'", m_frame->tree()->name().string().utf8().data(), m_URL.string().utf8().data(), 
        pdl ? pdl->url().string().utf8().data() : "<no provisional DocumentLoader>");

    // Check to see if we need to cache the page we are navigating away from into the back/forward cache.
    // We are doing this here because we know for sure that a new page is about to be loaded.
    cachePageForHistoryItem(history()->currentItem());
    
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
    
    if (cachedPage && cachedPage->document())
        open(*cachedPage);
    else {        
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

    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        history()->updateForClientRedirect();

    if (m_loadingFromCachedPage) {
        m_frame->document()->documentDidBecomeActive();
        
        // Force a layout to update view size and thereby update scrollbars.
        m_frame->view()->forceLayout();

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
        
        pageCache()->remove(history()->currentItem());

        m_documentLoader->setPrimaryLoadComplete(true);

        // FIXME: Why only this frame and not parent frames?
        checkLoadCompleteForThisFrame();
    }
}

void FrameLoader::transitionToCommitted(PassRefPtr<CachedPage> cachedPage)
{
    ASSERT(m_client->hasWebView());
    ASSERT(m_state == FrameStateProvisional);

    if (m_state != FrameStateProvisional)
        return;

    m_client->setCopiesOnScroll();
    history()->updateForCommit();

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
        case FrameLoadTypeBackWMLDeckNotAccessible:
        case FrameLoadTypeIndexedBackForward:
            if (Page* page = m_frame->page())
                if (page->backForwardList()) {
                    history()->updateForBackForwardNavigation();

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
            history()->updateForReload();
            m_client->transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeStandard:
            history()->updateForStandardLoad();
#ifndef BUILDING_ON_TIGER
            // This code was originally added for a Leopard performance imporvement. We decided to 
            // ifdef it to fix correctness issues on Tiger documented in <rdar://problem/5441823>.
            if (m_frame->view())
                m_frame->view()->setScrollbarsSuppressed(true);
#endif
            m_client->transitionToCommittedForNewPage();
            break;

        case FrameLoadTypeRedirectWithLockedBackForwardList:
            history()->updateForRedirectWithLockedBackForwardList();
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

    if (!m_client->hasHTMLView())
        receivedFirstData();
    else if (cachedPage) {
        // For non-cached HTML pages, these methods are called in receivedFirstData().
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

void FrameLoader::clientRedirected(const KURL& url, double seconds, double fireDate, bool lockBackForwardList)
{
    m_client->dispatchWillPerformClientRedirect(url, seconds, fireDate);
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    m_sentRedirectNotification = true;
    
    // If a "quick" redirect comes in, we set a special mode so we treat the next
    // load as part of the original navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    // Loads triggered by JavaScript form submissions never count as quick redirects.
    m_quickRedirectComing = lockBackForwardList && m_documentLoader && !m_isExecutingJavaScriptFormAction;
}

bool FrameLoader::shouldReload(const KURL& currentURL, const KURL& destinationURL)
{
#if ENABLE(WML)
    // All WML decks are supposed to be reloaded, even within the same URL fragment
    if (frameContainsWMLContent(m_frame))
        return true;
#endif

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
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->closeOldDataSources();
    
    if (m_documentLoader)
        m_client->dispatchWillClose();

    m_client->setMainFrameDocumentReady(false); // stop giving out the actual DOMDocument to observers
}

void FrameLoader::open(CachedPage& cachedPage)
{
    ASSERT(!m_frame->tree()->parent());
    ASSERT(m_frame->page());
    ASSERT(m_frame->page()->mainFrame() == m_frame);

    m_frame->redirectScheduler()->cancel();

    // We still have to close the previous part page.
    closeURL();
    
    // Delete old status bar messages (if it _was_ activated on last URL).
    if (m_frame->script()->isEnabled()) {
        m_frame->setJSStatusBarText(String());
        m_frame->setJSDefaultStatusBarText(String());
    }

    cachedPage.restore(m_frame->page());

    checkCompleted();
}

void FrameLoader::open(CachedFrameBase& cachedFrame)
{
    m_isComplete = false;
    
    // Don't re-emit the load event.
    m_didCallImplicitClose = true;

    KURL url = cachedFrame.url();

    if (url.protocolInHTTPFamily() && !url.host().isEmpty() && url.path().isEmpty())
        url.setPath("/");
    
    m_URL = url;
    m_workingURL = url;

    started();
    clear(true, true, cachedFrame.isMainFrame());

    Document* document = cachedFrame.document();
    ASSERT(document);
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
    if (m_frame->view())
        view->setFrameRect(m_frame->view()->frameRect());
    m_frame->setView(view);
    
    m_frame->setDocument(document);
    m_frame->setDOMWindow(cachedFrame.domWindow());
    m_frame->domWindow()->setURL(document->url());
    m_frame->domWindow()->setSecurityOrigin(document->securityOrigin());

    m_decoder = document->decoder();

    updateFirstPartyForCookies();

    cachedFrame.restore();
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

    m_responseMIMEType = mainResource->mimeType();
    didOpenURL(mainResource->url());

    String userChosenEncoding = documentLoader()->overrideEncoding();
    bool encodingIsUserChosen = !userChosenEncoding.isNull();
    setEncoding(encodingIsUserChosen ? userChosenEncoding : mainResource->textEncoding(), encodingIsUserChosen);

    ASSERT(m_frame->document());

    addData(mainResource->data()->data(), mainResource->data()->size());
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
    
CachePolicy FrameLoader::subresourceCachePolicy() const
{
    if (m_isComplete)
        return CachePolicyVerify;

    if (m_loadType == FrameLoadTypeReloadFromOrigin)
        return CachePolicyReload;

    if (Frame* parentFrame = m_frame->tree()->parent()) {
        CachePolicy parentCachePolicy = parentFrame->loader()->subresourceCachePolicy();
        if (parentCachePolicy != CachePolicyVerify)
            return parentCachePolicy;
    }

    // FIXME: POST documents are always Reloads, but their subresources should still be Revalidate.
    // If we bring the CachePolicy.h and ResourceRequest cache policy enums in sync with each other and
    // remember "Revalidate" in ResourceRequests, we can remove this "POST" check and return either "Reload" 
    // or "Revalidate" if the DocumentLoader was requested with either.
    const ResourceRequest& request(documentLoader()->request());
    if (request.cachePolicy() == ReloadIgnoringCacheData && !equalIgnoringCase(request.httpMethod(), "post"))
        return CachePolicyRevalidate;

    if (m_loadType == FrameLoadTypeReload)
        return CachePolicyRevalidate;

    return CachePolicyVerify;
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
                    item = history()->currentItem();
                
            bool shouldReset = true;
            if (!(pdl->isLoadingInAPISense() && !pdl->isStopping())) {
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
            if (!dl || (dl->isLoadingInAPISense() && !dl->isStopping()))
                return;

            markLoadComplete();

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            m_client->forceLayoutForNonHTML();
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if (Page* page = m_frame->page())
                if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadFromOrigin) && page->backForwardList())
                    history()->restoreScrollPositionAndViewState();

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

void FrameLoader::continueLoadAfterWillSubmitForm()
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

    m_loadingFromCachedPage = false;

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
            history()->restoreScrollPositionAndViewState();

    m_firstLayoutDone = true;
    m_client->dispatchDidFirstLayout();
}

void FrameLoader::didFirstVisuallyNonEmptyLayout()
{
    m_client->dispatchDidFirstVisuallyNonEmptyLayout();
}

void HistoryController::updateForFrameLoadCompleted()
{
    // Even if already complete, we might have set a previous item on a frame that
    // didn't do any data loading on the past transaction. Make sure to clear these out.
    m_previousItem = 0;
}

void FrameLoader::frameLoadCompleted()
{
    // Note: Can be called multiple times.

    m_client->frameLoadCompleted();

    history()->updateForFrameLoadCompleted();

    // After a canceled provisional load, firstLayoutDone is false.
    // Reset it to true if we're displaying a page.
    if (m_documentLoader)
        m_firstLayoutDone = true;
}

bool FrameLoader::firstLayoutDone() const
{
    return m_firstLayoutDone;
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

void FrameLoader::closeAndRemoveChild(Frame* child)
{
    child->tree()->detachFromParent();

    child->setView(0);
    if (child->ownerElement() && child->page())
        child->page()->decrementFrameCount();
    child->pageDestroyed();

    m_frame->tree()->removeChild(child);
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
    
    m_shouldCallCheckLoadComplete = false;

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

String FrameLoader::userAgent(const KURL& url) const
{
    return m_client->userAgent(url);
}

void FrameLoader::tokenizerProcessedData()
{
    checkCompleted();
}

void FrameLoader::handledOnloadEvents()
{
    m_client->dispatchDidHandleOnloadEvents();
}

void FrameLoader::frameDetached()
{
    stopAllLoaders();
    m_frame->document()->stopActiveDOMObjects();
    detachFromParent();
}

void FrameLoader::detachFromParent()
{
    RefPtr<Frame> protect(m_frame);

    closeURL();
    stopAllLoaders();
    history()->saveScrollPositionAndViewStateToItem(history()->currentItem());
    detachChildren();

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->frameDetachedFromParent(m_frame);
#endif

    detachViewsAndDocumentLoader();

    if (Frame* parent = m_frame->tree()->parent()) {
        parent->loader()->closeAndRemoveChild(m_frame);
        parent->loader()->scheduleCheckCompleted();
    } else {
        m_frame->setView(0);
        m_frame->pageDestroyed();
    }
}

void FrameLoader::detachViewsAndDocumentLoader()
{
    m_client->detachedFromParent2();
    setDocumentLoader(0);
    m_client->detachedFromParent3();
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
    // Don't set the cookie policy URL if it's already been set.
    // But make sure to set it on all requests, as it has significance beyond the cookie policy for all protocols (<rdar://problem/6616664>).
    if (request.firstPartyForCookies().isEmpty()) {
        if (mainResource && (isLoadingMainFrame() || cookiePolicyURLFromRequest))
            request.setFirstPartyForCookies(request.url());
        else if (Document* document = m_frame->document())
            request.setFirstPartyForCookies(document->firstPartyForCookies());
    }

    // The remaining modifications are only necessary for HTTP and HTTPS.
    if (!request.url().isEmpty() && !request.url().protocolInHTTPFamily())
        return;

    applyUserAgent(request);
    
    if (loadType == FrameLoadTypeReload) {
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "max-age=0");
    } else if (loadType == FrameLoadTypeReloadFromOrigin) {
        request.setCachePolicy(ReloadIgnoringCacheData);
        request.setHTTPHeaderField("Cache-Control", "no-cache");
        request.setHTTPHeaderField("Pragma", "no-cache");
    }
    
    if (mainResource)
        request.setHTTPAccept(defaultAcceptHeader);

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

void FrameLoader::loadPostRequest(const ResourceRequest& inRequest, const String& referrer, const String& frameName, bool lockHistory, FrameLoadType loadType, PassRefPtr<Event> event, PassRefPtr<FormState> prpFormState)
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
        // The search for a target frame is done earlier in the case of form submission.
        if (Frame* targetFrame = formState ? 0 : findFrameForNavigation(frameName))
            targetFrame->loader()->loadWithNavigationAction(workingResourceRequest, action, lockHistory, loadType, formState.release());
        else
            policyChecker()->checkNewWindowPolicy(action, FrameLoader::callContinueLoadAfterNewWindowPolicy, workingResourceRequest, formState.release(), frameName, this);
    } else
        loadWithNavigationAction(workingResourceRequest, action, lockHistory, loadType, formState.release());    
}

unsigned long FrameLoader::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    String referrer = m_outgoingReferrer;
    if (SecurityOrigin::shouldHideReferrer(request.url(), referrer))
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
        initialRequest.setFirstPartyForCookies(page->mainFrame()->loader()->documentLoader()->request().url());
    initialRequest.setHTTPUserAgent(client()->userAgent(request.url()));

    unsigned long identifier = 0;    
    ResourceRequest newRequest(initialRequest);
    requestFromDelegate(newRequest, identifier, error);

    if (error.isNull()) {
        ASSERT(!newRequest.isNull());
        
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
        if (!documentLoader()->applicationCacheHost()->maybeLoadSynchronously(newRequest, error, response, data)) {
#endif
            ResourceHandle::loadResourceSynchronously(newRequest, storedCredentials, error, response, data, m_frame);
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
            documentLoader()->applicationCacheHost()->maybeLoadFallbackSynchronously(newRequest, error, response, data);
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

void FrameLoader::didLoadResourceByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString)
{
    m_client->dispatchDidLoadResourceByXMLHttpRequest(identifier, sourceString);
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
        if (m_submittedFormURL == m_provisionalDocumentLoader->originalRequestCopy().url())
            m_submittedFormURL = KURL();
            
        // We might have made a page cache item, but now we're bailing out due to an error before we ever
        // transitioned to the new page (before WebFrameState == commit).  The goal here is to restore any state
        // so that the existing view (that wenever got far enough to replace) can continue being used.
        history()->invalidateCurrentItemCachedPage();
        
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
    bool isRedirect = m_quickRedirectComing || policyChecker()->loadType() == FrameLoadTypeRedirectWithLockedBackForwardList;
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

        history()->updateBackForwardListForFragmentScroll();
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
        && !m_frame->document()->isFrameSet();
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

    bool isTargetItem = history()->provisionalItem() ? history()->provisionalItem()->isTargetItem() : false;

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
        if ((isTargetItem || isLoadingMainFrame()) && isBackForwardLoadType(policyChecker()->loadType()))
            if (Page* page = m_frame->page()) {
                Frame* mainFrame = page->mainFrame();
                if (HistoryItem* resetItem = mainFrame->loader()->history()->currentItem()) {
                    page->backForwardList()->goToItem(resetItem);
                    Settings* settings = m_frame->settings();
                    page->setGlobalHistoryItem((!settings || settings->privateBrowsingEnabled()) ? 0 : resetItem);
                }
            }
        return;
    }

    FrameLoadType type = policyChecker()->loadType();
    stopAllLoaders();
    
    // <rdar://problem/6250856> - In certain circumstances on pages with multiple frames, stopAllLoaders()
    // might detach the current FrameLoader, in which case we should bail on this newly defunct load. 
    if (!m_frame->page())
        return;

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
    if (Page* page = m_frame->page()) {
        if (page->mainFrame() == m_frame)
            page->inspectorController()->resumeDebugger();
    }
#endif

    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_loadType = type;
    setState(FrameStateProvisional);

    setPolicyDocumentLoader(0);

    if (isBackForwardLoadType(type) && loadProvisionalItemFromCachedPage())
        return;

    if (formState)
        m_client->dispatchWillSubmitForm(&PolicyChecker::continueLoadAfterWillSubmitForm, formState);
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

    mainFrame->page()->setOpenedByDOM();
    mainFrame->loader()->m_client->dispatchShow();
    mainFrame->loader()->setOpener(frame.get());
    mainFrame->loader()->loadWithNavigationAction(request, NavigationAction(), false, FrameLoadTypeStandard, formState);
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

#if ENABLE(INSPECTOR)
    page->inspectorController()->didLoadResourceFromMemoryCache(m_documentLoader.get(), resource);
#endif

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

bool FrameLoader::shouldInterruptLoadForXFrameOptions(const String& content, const KURL& url)
{
    Frame* topFrame = m_frame->tree()->top();
    if (m_frame == topFrame)
        return false;

    if (equalIgnoringCase(content, "deny"))
        return true;

    if (equalIgnoringCase(content, "sameorigin")) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(url);
        if (!origin->isSameSchemeHostPort(topFrame->document()->securityOrigin()))
            return true;
    }

    return false;
}

void HistoryController::updateBackForwardListForFragmentScroll()
{
    updateBackForwardListClippedAtTarget(false);
}

bool FrameLoader::loadProvisionalItemFromCachedPage()
{
    RefPtr<CachedPage> cachedPage = pageCache()->get(history()->provisionalItem());
    if (!cachedPage || !cachedPage->document())
        return false;

    DocumentLoader *provisionalLoader = provisionalDocumentLoader();
    LOG(PageCache, "WebCorePageCache: FrameLoader %p loading provisional DocumentLoader %p with URL '%s' from CachedPage %p", this, provisionalLoader, provisionalLoader->url().string().utf8().data(), cachedPage.get());
    
    provisionalLoader->prepareForLoadStart();

    m_loadingFromCachedPage = true;

    provisionalLoader->setCommitted(true);
    commitProvisionalLoad(cachedPage);
    
    return true;
}

void FrameLoader::cachePageForHistoryItem(HistoryItem* item)
{
    if (!canCachePage() || item->isInPageCache())
        return;

    pageHidden();
    
    if (Page* page = m_frame->page()) {
        RefPtr<CachedPage> cachedPage = CachedPage::create(page);
        pageCache()->add(item, cachedPage.release());
    }
}

void FrameLoader::pageHidden()
{
    m_unloadEventBeingDispatched = true;
    if (m_frame->domWindow())
        m_frame->domWindow()->dispatchEvent(PageTransitionEvent::create(EventNames().pagehideEvent, true), m_frame->document());
    m_unloadEventBeingDispatched = false;

    // Send pagehide event for subframes as well
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->loader()->pageHidden();
}

bool FrameLoader::shouldTreatURLAsSameAsCurrent(const KURL& url) const
{
    if (!history()->currentItem())
        return false;
    return url == history()->currentItem()->url() || url == history()->currentItem()->originalURL();
}

PassRefPtr<HistoryItem> HistoryController::createItem(bool useOriginal)
{
    DocumentLoader* docLoader = m_frame->loader()->documentLoader();
    
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
    m_previousItem = m_currentItem;
    m_currentItem = item;
    
    return item.release();
}
    
void FrameLoader::checkDidPerformFirstNavigation()
{
    Page* page = m_frame->page();
    if (!page)
        return;

    if (!m_didPerformFirstNavigation && page->backForwardList()->entries().size() == 1) {
        m_didPerformFirstNavigation = true;
        m_client->didPerformFirstNavigation();
    }
}

void HistoryController::updateBackForwardListClippedAtTarget(bool doClip)
{
    // In the case of saving state about a page with frames, we store a tree of items that mirrors the frame tree.  
    // The item that was the target of the user's navigation is designated as the "targetItem".  
    // When this function is called with doClip=true we're able to create the whole tree except for the target's children, 
    // which will be loaded in the future. That part of the tree will be filled out as the child loads are committed.

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame->loader()->documentLoader()->urlForHistory().isEmpty())
        return;

    Frame* mainFrame = page->mainFrame();
    ASSERT(mainFrame);
    FrameLoader* frameLoader = mainFrame->loader();

    frameLoader->checkDidPerformFirstNavigation();

    RefPtr<HistoryItem> item = frameLoader->history()->createItemTree(m_frame, doClip);
    LOG(BackForward, "WebCoreBackForward - Adding backforward item %p for frame %s", item.get(), m_frame->loader()->documentLoader()->url().string().ascii().data());
    page->backForwardList()->addItem(item);
}

PassRefPtr<HistoryItem> HistoryController::createItemTree(Frame* targetFrame, bool clipAtTarget)
{
    RefPtr<HistoryItem> bfItem = createItem(m_frame->tree()->parent() ? true : false);
    if (m_previousItem)
        saveScrollPositionAndViewStateToItem(m_previousItem.get());
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
                bfItem->addChildItem(childLoader->history()->createItemTree(targetFrame, clipAtTarget));
        }
    }
    if (m_frame == targetFrame)
        bfItem->setIsTargetItem(true);
    return bfItem;
}

Frame* FrameLoader::findFrameForNavigation(const AtomicString& name)
{
    Frame* frame = m_frame->tree()->find(name);
    if (!shouldAllowNavigation(frame))
        return 0;  
    return frame;
}

void HistoryController::saveScrollPositionAndViewStateToItem(HistoryItem* item)
{
    if (!item || !m_frame->view())
        return;
        
    item->setScrollPoint(m_frame->view()->scrollPosition());
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling through to the client.
    m_frame->loader()->client()->saveViewStateToItem(item);
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
void HistoryController::restoreScrollPositionAndViewState()
{
    if (!m_frame->loader()->committedFirstRealDocumentLoad())
        return;

    ASSERT(m_currentItem);
    
    // FIXME: As the ASSERT attests, it seems we should always have a currentItem here.
    // One counterexample is <rdar://problem/4917290>
    // For now, to cover this issue in release builds, there is no technical harm to returning
    // early and from a user standpoint - as in the above radar - the previous page load failed 
    // so there *is* no scroll or view state to restore!
    if (!m_currentItem)
        return;
    
    // FIXME: It would be great to work out a way to put this code in WebCore instead of calling
    // through to the client. It's currently used only for the PDF view on Mac.
    m_frame->loader()->client()->restoreViewState();
    
    if (FrameView* view = m_frame->view())
        if (!view->wasScrolledByUser())
            view->setScrollPosition(m_currentItem->scrollPoint());
}

void HistoryController::invalidateCurrentItemCachedPage()
{
    // When we are pre-commit, the currentItem is where the pageCache data resides    
    CachedPage* cachedPage = pageCache()->get(currentItem());

    // FIXME: This is a grotesque hack to fix <rdar://problem/4059059> Crash in RenderFlow::detach
    // Somehow the PageState object is not properly updated, and is holding onto a stale document.
    // Both Xcode and FileMaker see this crash, Safari does not.
    
    ASSERT(!cachedPage || cachedPage->document() == m_frame->document());
    if (cachedPage && cachedPage->document() == m_frame->document()) {
        cachedPage->document()->setInPageCache(false);
        cachedPage->clear();
    }
    
    if (cachedPage)
        pageCache()->remove(currentItem());
}

void HistoryController::saveDocumentState()
{
    // FIXME: Reading this bit of FrameLoader state here is unfortunate.  I need to study
    // this more to see if we can remove this dependency.
    if (m_frame->loader()->creatingInitialEmptyDocument())
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

    HistoryItem* item = m_previousItem ? m_previousItem.get() : m_currentItem.get();
    if (!item)
        return;

    Document* document = m_frame->document();
    ASSERT(document);
    
    if (item->isCurrentDocument(document)) {
        LOG(Loading, "WebCoreLoading %s: saving form state to %p", m_frame->tree()->name().string().utf8().data(), item);
        item->setDocumentState(document->formElementsState());
    }
}

// Loads content into this frame, as specified by history item
// FIXME: This function should really be split into a couple pieces, some of
// which should be methods of HistoryController and some of which should be
// methods of FrameLoader.
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
    // We also do not do anchor-style navigation if we're posting a form or navigating from
    // a page that was resulted from a form post.
    bool shouldScroll = !formData && !(history()->currentItem() && history()->currentItem()->formData()) && history()->urlsMatchItem(item);

#if ENABLE(WML)
    // All WML decks should go through the real load mechanism, not the scroll-to-anchor code
    if (frameContainsWMLContent(m_frame))
        shouldScroll = false;
#endif

    if (shouldScroll) {
        // Must do this maintenance here, since we don't go through a real page reload
        history()->saveScrollPositionAndViewStateToItem(history()->currentItem());

        if (FrameView* view = m_frame->view())
            view->setWasScrolledByUser(false);

        history()->setCurrentItem(item);

        // FIXME: Form state might need to be saved here too.

        // We always call scrollToAnchor here, even if the URL doesn't have an
        // anchor fragment. This is so we'll keep the WebCore Frame's URL up-to-date.
        scrollToAnchor(item->url());
    
        // must do this maintenance here, since we don't go through a real page reload
        history()->restoreScrollPositionAndViewState();
        
        // Fake the URL change by updating the data source's request.  This will no longer
        // be necessary if we do the better fix described above.
        documentLoader()->replaceRequestURLForAnchorScroll(itemURL);

        m_client->dispatchDidChangeLocationWithinPage();
        
        // FrameLoaderClient::didFinishLoad() tells the internal load delegate the load finished with no error
        m_client->didFinishLoad();
    } else {
        // Remember this item so we can traverse any child items as child frames load
        history()->setProvisionalItem(item);

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
                LOG(PageCache, "Not restoring page for %s from back/forward cache because cache entry has expired", history()->provisionalItem()->url().string().ascii().data());
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
                
                if (ResourceHandle::willLoadFromCache(request, m_frame))
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
                    case FrameLoadTypeBackWMLDeckNotAccessible:
                    case FrameLoadTypeForward:
                    case FrameLoadTypeIndexedBackForward:
                        if (itemURL.protocol() != "https")
                            request.setCachePolicy(ReturnCacheDataElseLoad);
                        break;
                    case FrameLoadTypeStandard:
                    case FrameLoadTypeRedirectWithLockedBackForwardList:
                        break;
                    case FrameLoadTypeSame:
                    default:
                        ASSERT_NOT_REACHED();
                }

                action = NavigationAction(itemOriginalURL, loadType, false);
            }
            
            if (!addedExtraFields)
                addExtraFieldsToRequest(request, m_loadType, true, formData);

            loadWithNavigationAction(request, action, false, loadType, 0);
        }
    }
}

// Walk the frame tree and ensure that the URLs match the URLs in the item.
bool HistoryController::urlsMatchItem(HistoryItem* item) const
{
    const KURL& currentURL = m_frame->loader()->documentLoader()->url();
    if (!equalIgnoringFragmentIdentifier(currentURL, item->url()))
        return false;

    const HistoryItemVector& childItems = item->children();

    unsigned size = childItems.size();
    for (unsigned i = 0; i < size; ++i) {
        Frame* childFrame = m_frame->tree()->child(childItems[i]->target());
        if (childFrame && !childFrame->loader()->history()->urlsMatchItem(childItems[i].get()))
            return false;
    }

    return true;
}

// Main funnel for navigating to a previous location (back/forward, non-search snap-back)
// This includes recursion to handle loading into framesets properly
void HistoryController::goToItem(HistoryItem* targetItem, FrameLoadType type)
{
    ASSERT(!m_frame->tree()->parent());
    
    // shouldGoToHistoryItem is a private delegate method. This is needed to fix:
    // <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
    // Ultimately, history item navigations should go through the policy delegate. That's covered in:
    // <rdar://problem/3979539> back/forward cache navigations should consult policy delegate
    Page* page = m_frame->page();
    if (!page)
        return;
    if (!m_frame->loader()->client()->shouldGoToHistoryItem(targetItem))
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
void HistoryController::recursiveGoToItem(HistoryItem* item, HistoryItem* fromItem, FrameLoadType type)
{
    ASSERT(item);
    ASSERT(fromItem);

    KURL itemURL = item->url();
    KURL currentURL;
    if (m_frame->loader()->documentLoader())
        currentURL = m_frame->loader()->documentLoader()->url();

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
        ASSERT(!m_previousItem);
        saveDocumentState();
        saveScrollPositionAndViewStateToItem(m_currentItem.get());

        if (FrameView* view = m_frame->view())
            view->setWasScrolledByUser(false);

        m_currentItem = item;
                
        // Restore form state (works from currentItem)
        restoreDocumentState();
        
        // Restore the scroll position (we choose to do this rather than going back to the anchor point)
        restoreScrollPositionAndViewState();
        
        const HistoryItemVector& childItems = item->children();
        
        int size = childItems.size();
        for (int i = 0; i < size; ++i) {
            String childFrameName = childItems[i]->target();
            HistoryItem* fromChildItem = fromItem->childItemWithTarget(childFrameName);
            ASSERT(fromChildItem || fromItem->isTargetItem());
            Frame* childFrame = m_frame->tree()->child(childFrameName);
            ASSERT(childFrame);
            childFrame->loader()->history()->recursiveGoToItem(childItems[i].get(), fromChildItem, type);
        }
    } else {
        m_frame->loader()->loadItem(item, type);
    }
}

// helper method that determines whether the subframes described by the item's subitems
// match our own current frameset
bool HistoryController::childFramesMatchItem(HistoryItem* item) const
{
    const HistoryItemVector& childItems = item->children();
    if (childItems.size() != m_frame->tree()->childCount())
        return false;
    
    unsigned size = childItems.size();
    for (unsigned i = 0; i < size; ++i) {
        if (!m_frame->tree()->child(childItems[i]->target()))
            return false;
    }
    
    // Found matches for all item targets
    return true;
}

// There are 3 things you might think of as "history", all of which are handled by these functions.
//
//     1) Back/forward: The m_currentItem is part of this mechanism.
//     2) Global history: Handled by the client.
//     3) Visited links: Handled by the PageGroup.

void HistoryController::updateForStandardLoad()
{
    LOG(History, "WebCoreHistory: Updating History for Standard Load in frame %s", m_frame->loader()->documentLoader()->url().string().ascii().data());

    FrameLoader* frameLoader = m_frame->loader();

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = frameLoader->documentLoader()->urlForHistory();

    if (!frameLoader->documentLoader()->isClientRedirect()) {
        if (!historyURL.isEmpty()) {
            updateBackForwardListClippedAtTarget(true);
            if (!needPrivacy) {
                frameLoader->client()->updateGlobalHistory();
                frameLoader->documentLoader()->setDidCreateGlobalHistoryEntry(true);
                if (frameLoader->documentLoader()->unreachableURL().isEmpty())
                    frameLoader->client()->updateGlobalHistoryRedirectLinks();
            }
            if (Page* page = m_frame->page())
                page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
        }
    } else if (frameLoader->documentLoader()->unreachableURL().isEmpty() && m_currentItem) {
        m_currentItem->setURL(frameLoader->documentLoader()->url());
        m_currentItem->setFormInfoFromRequest(frameLoader->documentLoader()->request());
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);

        if (!frameLoader->documentLoader()->didCreateGlobalHistoryEntry() && frameLoader->documentLoader()->unreachableURL().isEmpty() && !frameLoader->url().isEmpty())
            frameLoader->client()->updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForClientRedirect()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for client redirect in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    // Clear out form data so we don't try to restore it into the incoming page.  Must happen after
    // webcore has closed the URL and saved away the form state.
    if (m_currentItem) {
        m_currentItem->clearDocumentState();
        m_currentItem->clearScrollPoint();
    }

    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = m_frame->loader()->documentLoader()->urlForHistory();

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);
    }
}

void HistoryController::updateForBackForwardNavigation()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for back/forward navigation in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    // Must grab the current scroll position before disturbing it
    saveScrollPositionAndViewStateToItem(m_previousItem.get());
}

void HistoryController::updateForReload()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for reload in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif

    if (m_currentItem) {
        pageCache()->remove(m_currentItem.get());
    
        if (m_frame->loader()->loadType() == FrameLoadTypeReload || m_frame->loader()->loadType() == FrameLoadTypeReloadFromOrigin)
            saveScrollPositionAndViewStateToItem(m_currentItem.get());
    
        // Sometimes loading a page again leads to a different result because of cookies. Bugzilla 4072
        if (m_frame->loader()->documentLoader()->unreachableURL().isEmpty())
            m_currentItem->setURL(m_frame->loader()->documentLoader()->requestURL());
    }
}

void HistoryController::updateForRedirectWithLockedBackForwardList()
{
#if !LOG_DISABLED
    if (m_frame->loader()->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for redirect load in frame %s", m_frame->loader()->documentLoader()->title().utf8().data());
#endif
    
    Settings* settings = m_frame->settings();
    bool needPrivacy = !settings || settings->privateBrowsingEnabled();
    const KURL& historyURL = m_frame->loader()->documentLoader()->urlForHistory();

    if (m_frame->loader()->documentLoader()->isClientRedirect()) {
        if (!m_currentItem && !m_frame->tree()->parent()) {
            if (!historyURL.isEmpty()) {
                updateBackForwardListClippedAtTarget(true);
                if (!needPrivacy) {
                    m_frame->loader()->client()->updateGlobalHistory();
                    m_frame->loader()->documentLoader()->setDidCreateGlobalHistoryEntry(true);
                    if (m_frame->loader()->documentLoader()->unreachableURL().isEmpty())
                        m_frame->loader()->client()->updateGlobalHistoryRedirectLinks();
                }
                if (Page* page = m_frame->page())
                    page->setGlobalHistoryItem(needPrivacy ? 0 : page->backForwardList()->currentItem());
            }
        }
        if (m_currentItem) {
            m_currentItem->setURL(m_frame->loader()->documentLoader()->url());
            m_currentItem->setFormInfoFromRequest(m_frame->loader()->documentLoader()->request());
        }
    } else {
        Frame* parentFrame = m_frame->tree()->parent();
        if (parentFrame && parentFrame->loader()->history()->m_currentItem)
            parentFrame->loader()->history()->m_currentItem->setChildItem(createItem(true));
    }

    if (!historyURL.isEmpty() && !needPrivacy) {
        if (Page* page = m_frame->page())
            page->group().addVisitedLink(historyURL);

        if (!m_frame->loader()->documentLoader()->didCreateGlobalHistoryEntry() && m_frame->loader()->documentLoader()->unreachableURL().isEmpty() && !m_frame->loader()->url().isEmpty())
            m_frame->loader()->client()->updateGlobalHistoryRedirectLinks();
    }
}

void HistoryController::updateForCommit()
{
    FrameLoader* frameLoader = m_frame->loader();
#if !LOG_DISABLED
    if (frameLoader->documentLoader())
        LOG(History, "WebCoreHistory: Updating History for commit in frame %s", frameLoader->documentLoader()->title().utf8().data());
#endif
    FrameLoadType type = frameLoader->loadType();
    if (isBackForwardLoadType(type) ||
        ((type == FrameLoadTypeReload || type == FrameLoadTypeReloadFromOrigin) && !frameLoader->provisionalDocumentLoader()->unreachableURL().isEmpty())) {
        // Once committed, we want to use current item for saving DocState, and
        // the provisional item for restoring state.
        // Note previousItem must be set before we close the URL, which will
        // happen when the data source is made non-provisional below
        m_previousItem = m_currentItem;
        ASSERT(m_provisionalItem);
        m_currentItem = m_provisionalItem;
        m_provisionalItem = 0;
    }
}

void HistoryController::updateForAnchorScroll()
{
    if (m_frame->loader()->url().isEmpty())
        return;

    Settings* settings = m_frame->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    page->group().addVisitedLink(m_frame->loader()->url());
}

// Walk the frame tree, telling all frames to save their form state into their current
// history item.
void HistoryController::saveDocumentAndScrollState()
{
    for (Frame* frame = m_frame; frame; frame = frame->tree()->traverseNext(m_frame)) {
        frame->loader()->history()->saveDocumentState();
        frame->loader()->history()->saveScrollPositionAndViewStateToItem(frame->loader()->history()->currentItem());
    }
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

void FrameLoader::dispatchDocumentElementAvailable()
{
    m_frame->injectUserScripts(InjectAtDocumentStart);
    m_client->documentElementAvailable();
}

void FrameLoader::dispatchWindowObjectAvailable()
{
    if (!m_frame->script()->isEnabled() || !m_frame->script()->haveWindowShell())
        return;

    m_client->windowObjectCleared();

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page()) {
        if (InspectorController* inspector = page->inspectorController())
            inspector->inspectedWindowScriptObjectCleared(m_frame);
        if (InspectorController* inspector = page->parentInspectorController())
            inspector->windowScriptObjectAvailable();
    }
#endif
}

PassRefPtr<Widget> FrameLoader::createJavaAppletWidget(const IntSize& size, HTMLAppletElement* element, const HashMap<String, String>& args)
{
    String baseURLString;
    String codeBaseURLString;
    Vector<String> paramNames;
    Vector<String> paramValues;
    HashMap<String, String>::const_iterator end = args.end();
    for (HashMap<String, String>::const_iterator it = args.begin(); it != end; ++it) {
        if (equalIgnoringCase(it->first, "baseurl"))
            baseURLString = it->second;
        else if (equalIgnoringCase(it->first, "codebase"))
            codeBaseURLString = it->second;
        paramNames.append(it->first);
        paramValues.append(it->second);
    }

    if (!codeBaseURLString.isEmpty()) {
        KURL codeBaseURL = completeURL(codeBaseURLString);
        if (!SecurityOrigin::canLoad(codeBaseURL, String(), element->document())) {
            FrameLoader::reportLocalLoadFailed(m_frame, codeBaseURL.string());
            return 0;
        }
    }

    if (baseURLString.isEmpty())
        baseURLString = m_frame->document()->baseURL().string();
    KURL baseURL = completeURL(baseURLString);

    RefPtr<Widget> widget = m_client->createJavaAppletWidget(size, element, baseURL, paramNames, paramValues);
    if (!widget)
        return 0;

    m_containsPlugIns = true;
    return widget;
}

void HistoryController::setCurrentItemTitle(const String& title)
{
    if (m_currentItem)
        m_currentItem->setTitle(title);
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    m_client->didChangeTitle(loader);

    if (loader == m_documentLoader) {
        // Must update the entries in the back-forward list too.
        history()->setCurrentItemTitle(loader->title());
        // This must go through the WebFrame because it has the right notion of the current b/f item.
        m_client->setTitle(loader->title(), loader->urlForHistory());
        m_client->setMainFrameDocumentReady(true); // update observers with new DOMDocument
        m_client->dispatchDidReceiveTitle(loader->title());
    }
}

void FrameLoader::dispatchDidCommitLoad()
{
    if (m_creatingInitialEmptyDocument)
        return;

#ifndef NDEBUG
    m_didDispatchDidCommitLoad = true;
#endif

    m_client->dispatchDidCommitLoad();

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->didCommitLoad(m_documentLoader.get());
#endif
}

void FrameLoader::dispatchAssignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    m_client->assignIdentifierToInitialRequest(identifier, loader, request);

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->identifierForInitialRequest(identifier, loader, request);
#endif
}

void FrameLoader::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    StringImpl* oldRequestURL = request.url().string().impl();
    m_documentLoader->didTellClientAboutLoad(request.url());

    m_client->dispatchWillSendRequest(loader, identifier, request, redirectResponse);

    // If the URL changed, then we want to put that new URL in the "did tell client" set too.
    if (!request.isNull() && oldRequestURL != request.url().string().impl())
        m_documentLoader->didTellClientAboutLoad(request.url());

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->willSendRequest(loader, identifier, request, redirectResponse);
#endif
}

void FrameLoader::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    m_client->dispatchDidReceiveResponse(loader, identifier, r);

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->didReceiveResponse(loader, identifier, r);
#endif
}

void FrameLoader::dispatchDidReceiveContentLength(DocumentLoader* loader, unsigned long identifier, int length)
{
    m_client->dispatchDidReceiveContentLength(loader, identifier, length);

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->didReceiveContentLength(loader, identifier, length);
#endif
}

void FrameLoader::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier)
{
    m_client->dispatchDidFinishLoading(loader, identifier);

#if ENABLE(INSPECTOR)
    if (Page* page = m_frame->page())
        page->inspectorController()->didFinishLoading(loader, identifier);
#endif
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

bool FrameLoaderClient::hasHTMLView() const
{
    return true;
}

} // namespace WebCore
