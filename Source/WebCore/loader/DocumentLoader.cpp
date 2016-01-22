/*
 * Copyright (C) 2006-2008, 2016 Apple Inc. All rights reserved.
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
#include "DocumentLoader.h"

#include "ApplicationCacheHost.h"
#include "ArchiveResourceCollection.h"
#include "CachedPage.h"
#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "ContentExtensionError.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentParser.h"
#include "DocumentWriter.h"
#include "Event.h"
#include "ExtensionStyleSheets.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTTPHeaderNames.h"
#include "HistoryItem.h"
#include "IconController.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "SchemeRegistry.h"
#include "ScriptController.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include <wtf/Assertions.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "ArchiveFactory.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilter.h"
#endif

namespace WebCore {

static void cancelAll(const ResourceLoaderMap& loaders)
{
    Vector<RefPtr<ResourceLoader>> loadersCopy;
    copyValuesToVector(loaders, loadersCopy);
    for (auto& loader : loadersCopy)
        loader->cancel();
}

static void setAllDefersLoading(const ResourceLoaderMap& loaders, bool defers)
{
    Vector<RefPtr<ResourceLoader>> loadersCopy;
    copyValuesToVector(loaders, loadersCopy);
    for (auto& loader : loadersCopy)
        loader->setDefersLoading(defers);
}

static bool areAllLoadersPageCacheAcceptable(const ResourceLoaderMap& loaders)
{
    Vector<RefPtr<ResourceLoader>> loadersCopy;
    copyValuesToVector(loaders, loadersCopy);
    for (auto& loader : loadersCopy) {
        if (!loader->frameLoader() || !loader->frameLoader()->frame().page())
            return false;

        CachedResource* cachedResource = MemoryCache::singleton().resourceForRequest(loader->request(), loader->frameLoader()->frame().page()->sessionID());
        if (!cachedResource)
            return false;

        // Only image and XHR loads do prevent the page from entering the PageCache.
        // All non-image loads will prevent the page from entering the PageCache.
        if (!cachedResource->isImage() && !cachedResource->areAllClientsXMLHttpRequests())
            return false;
    }
    return true;
}

DocumentLoader::DocumentLoader(const ResourceRequest& req, const SubstituteData& substituteData)
    : m_deferMainResourceDataLoad(true)
    , m_frame(nullptr)
    , m_cachedResourceLoader(CachedResourceLoader::create(this))
    , m_writer(m_frame)
    , m_originalRequest(req)
    , m_substituteData(substituteData)
    , m_originalRequestCopy(req)
    , m_request(req)
    , m_originalSubstituteDataWasValid(substituteData.isValid())
    , m_committed(false)
    , m_isStopping(false)
    , m_gotFirstByte(false)
    , m_isClientRedirect(false)
    , m_isLoadingMultipartContent(false)
    , m_wasOnloadDispatched(false)
    , m_stopRecordingResponses(false)
    , m_substituteResourceDeliveryTimer(*this, &DocumentLoader::substituteResourceDeliveryTimerFired)
    , m_didCreateGlobalHistoryEntry(false)
    , m_loadingMainResource(false)
    , m_timeOfLastDataReceived(0.0)
    , m_identifierForLoadWithoutResourceLoader(0)
    , m_dataLoadTimer(*this, &DocumentLoader::handleSubstituteDataLoadNow)
    , m_subresourceLoadersArePageCacheAcceptable(false)
    , m_applicationCacheHost(std::make_unique<ApplicationCacheHost>(*this))
{
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->loader();
}

SubresourceLoader* DocumentLoader::mainResourceLoader() const
{
    return m_mainResource ? m_mainResource->loader() : 0;
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || frameLoader()->activeDocumentLoader() != this || !isLoading());
    ASSERT_WITH_MESSAGE(!m_waitingForContentPolicy, "The content policy callback should never outlive its DocumentLoader.");
    ASSERT_WITH_MESSAGE(!m_waitingForNavigationPolicy, "The navigation policy callback should never outlive its DocumentLoader.");
    if (m_iconLoadDecisionCallback)
        m_iconLoadDecisionCallback->invalidate();
    if (m_iconDataCallback)
        m_iconDataCallback->invalidate();
    m_cachedResourceLoader->clearDocumentLoader();
    
    clearMainResource();
}

PassRefPtr<SharedBuffer> DocumentLoader::mainResourceData() const
{
    if (m_substituteData.isValid())
        return m_substituteData.content()->copy();
    if (m_mainResource)
        return m_mainResource->resourceBuffer();
    return nullptr;
}

Document* DocumentLoader::document() const
{
    if (m_frame && m_frame->loader().documentLoader() == this)
        return m_frame->document();
    return nullptr;
}

const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

const ResourceRequest& DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy;
}

const ResourceRequest& DocumentLoader::request() const
{
    return m_request;
}

ResourceRequest& DocumentLoader::request()
{
    return m_request;
}

const URL& DocumentLoader::url() const
{
    return request().url();
}

void DocumentLoader::replaceRequestURLForSameDocumentNavigation(const URL& url)
{
    m_originalRequestCopy.setURL(url);
    m_request.setURL(url);
}

void DocumentLoader::setRequest(const ResourceRequest& req)
{
    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    bool handlingUnreachableURL = false;

    handlingUnreachableURL = m_substituteData.isValid() && !m_substituteData.failingURL().isEmpty();

    if (handlingUnreachableURL)
        m_committed = false;

    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!m_committed);

    m_request = req;
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    m_mainDocumentError = error;    
    frameLoader()->client().setMainDocumentError(this, error);
}

void DocumentLoader::mainReceivedError(const ResourceError& error)
{
    ASSERT(!error.isNull());

    if (m_identifierForLoadWithoutResourceLoader) {
        ASSERT(!mainResourceLoader());
        frameLoader()->client().dispatchDidFailLoading(this, m_identifierForLoadWithoutResourceLoader, error);
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    m_applicationCacheHost->failedLoadingMainResource();

    if (!frameLoader())
        return;
    setMainDocumentError(error);
    clearMainResourceLoader();
    frameLoader()->receivedMainResourceError(error);
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    RefPtr<Frame> protectFrame(m_frame);
    Ref<DocumentLoader> protectLoader(*this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();

    // We may want to audit the existing subresource loaders when we are on a page which has completed
    // loading but there are subresource loads during cancellation. This must be done before the
    // frame->stopLoading() call, which may evict the CachedResources, which we rely on to check
    // the type of the resource loads.
    if (loading && m_committed && !mainResourceLoader() && !m_subresourceLoaders.isEmpty())
        m_subresourceLoadersArePageCacheAcceptable = areAllLoadersPageCacheAcceptable(m_subresourceLoaders);

    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();
        
        if (loading || doc->parsing())
            m_frame->loader().stopLoading(UnloadEventPolicyNone);
    }

    // Always cancel multipart loaders
    cancelAll(m_multipartSubresourceLoaders);

    // Appcache uses ResourceHandle directly, DocumentLoader doesn't count these loads.
    m_applicationCacheHost->stopLoadingInFrame(m_frame);
    
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    clearArchiveResources();
#endif

    if (!loading) {
        // If something above restarted loading we might run into mysterious crashes like 
        // https://bugs.webkit.org/show_bug.cgi?id=62764 and <rdar://problem/9328684>
        ASSERT(!isLoading());
        return;
    }

    // We might run in to infinite recursion if we're stopping loading as the result of 
    // detaching from the frame, so break out of that recursion here.
    // See <rdar://problem/9673866> for more details.
    if (m_isStopping)
        return;

    m_isStopping = true;

    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    
    if (isLoadingMainResource()) {
        // Stop the main resource loader and let it send the cancelled message.
        cancelMainResourceLoad(frameLoader->cancelledError(m_request));
    } else if (!m_subresourceLoaders.isEmpty() || !m_plugInStreamLoaders.isEmpty()) {
        // The main resource loader already finished loading. Set the cancelled error on the
        // document and let the subresourceLoaders and pluginLoaders send individual cancelled messages below.
        setMainDocumentError(frameLoader->cancelledError(m_request));
    } else {
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        mainReceivedError(frameLoader->cancelledError(m_request));
    }

    // We always need to explicitly cancel the Document's parser when stopping the load.
    // Otherwise cancelling the parser while starting the next page load might result
    // in unexpected side effects such as erroneous event dispatch. ( http://webkit.org/b/117112 )
    if (Document* document = this->document())
        document->cancelParsing();
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
    
    m_isStopping = false;
}

void DocumentLoader::commitIfReady()
{
    if (!m_committed) {
        m_committed = true;
        frameLoader()->commitProvisionalLoad();
    }
}

bool DocumentLoader::isLoading() const
{
    // if (document() && document()->hasActiveParser())
    //     return true;
    // FIXME: The above code should be enabled, but it seems to cause
    // http/tests/security/feed-urls-from-remote.html to timeout on Mac WK1
    // see http://webkit.org/b/110554 and http://webkit.org/b/110401

    return isLoadingMainResource() || !m_subresourceLoaders.isEmpty() || !m_plugInStreamLoaders.isEmpty();
}

void DocumentLoader::notifyFinished(CachedResource* resource)
{
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterNotifyFinished(resource))
        return;
#endif

    ASSERT_UNUSED(resource, m_mainResource == resource);
    ASSERT(m_mainResource);
    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading(m_mainResource->loadFinishTime());
        return;
    }

    if (m_request.cachePolicy() == ReturnCacheDataDontLoad && !m_mainResource->wasCanceled()) {
        frameLoader()->retryAfterFailedCacheOnlyMainResourceLoad();
        return;
    }

    mainReceivedError(m_mainResource->resourceError());
}

void DocumentLoader::finishedLoading(double finishTime)
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!m_frame->page()->defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame));
#endif

    Ref<DocumentLoader> protect(*this);

    if (m_identifierForLoadWithoutResourceLoader) {
        // A didFinishLoading delegate might try to cancel the load (despite it
        // being finished). Clear m_identifierForLoadWithoutResourceLoader
        // before calling dispatchDidFinishLoading so that we don't later try to
        // cancel the already-finished substitute load.
        unsigned long identifier = m_identifierForLoadWithoutResourceLoader;
        m_identifierForLoadWithoutResourceLoader = 0;
        frameLoader()->notifier().dispatchDidFinishLoading(this, identifier, finishTime);
    }

    maybeFinishLoadingMultipartContent();

    double responseEndTime = finishTime;
    if (!responseEndTime)
        responseEndTime = m_timeOfLastDataReceived;
    if (!responseEndTime)
        responseEndTime = monotonicallyIncreasingTime();
    timing().setResponseEnd(responseEndTime);

    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_gotFirstByte)
            commitData(0, 0);
        frameLoader()->client().finishedLoading(this);
    }

    m_writer.end();
    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();
    if (!frameLoader()->stateMachine().creatingInitialEmptyDocument())
        frameLoader()->checkLoadComplete();

    // If the document specified an application cache manifest, it violates the author's intent if we store it in the memory cache
    // and deny the appcache the chance to intercept it in the future, so remove from the memory cache.
    if (m_frame) {
        if (m_mainResource && m_frame->document()->hasManifest())
            MemoryCache::singleton().remove(*m_mainResource);
    }
    m_applicationCacheHost->finishedLoadingMainResource();
}

bool DocumentLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST")
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && m_originalRequest.httpMethod() == "POST")
        return true;

    return false;
}

void DocumentLoader::handleSubstituteDataLoadNow()
{
    ResourceResponse response = m_substituteData.response();
    if (response.url().isEmpty())
        response = ResourceResponse(m_request.url(), m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding());

    responseReceived(0, response);
}

void DocumentLoader::startDataLoadTimer()
{
    m_dataLoadTimer.startOneShot(0);

#if HAVE(RUNLOOP_TIMER)
    if (SchedulePairHashSet* scheduledPairs = m_frame->page()->scheduledRunLoopPairs())
        m_dataLoadTimer.schedule(*scheduledPairs);
#endif
}

void DocumentLoader::handleSubstituteDataLoadSoon()
{
    if (!m_deferMainResourceDataLoad || frameLoader()->loadsSynchronously())
        handleSubstituteDataLoadNow();
    else
        startDataLoadTimer();
}

void DocumentLoader::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(resource, resource == m_mainResource);
    willSendRequest(request, redirectResponse);
}

void DocumentLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());

    if (!frameLoader()->checkIfFormActionAllowedByCSP(newRequest.url())) {
        cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
        return;
    }

    ASSERT(timing().fetchStart());
    if (!redirectResponse.isNull()) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        Ref<SecurityOrigin> redirectingOrigin(SecurityOrigin::create(redirectResponse.url()));
        if (!redirectingOrigin.get().canDisplay(newRequest.url())) {
            FrameLoader::reportLocalLoadFailed(m_frame, newRequest.url().string());
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return;
        }
        if (!portAllowed(newRequest.url())) {
            FrameLoader::reportBlockedPortFailed(m_frame, newRequest.url().string());
            cancelMainResourceLoad(frameLoader()->blockedError(newRequest));
            return;
        }
        timing().addRedirect(redirectResponse.url(), newRequest.url());
    }

    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->frame().isMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadIgnoringCacheData);

    Frame& topFrame = m_frame->tree().top();
    if (&topFrame != m_frame) {
        if (!frameLoader()->mixedContentChecker().canDisplayInsecureContent(topFrame.document()->securityOrigin(), MixedContentChecker::ContentType::Active, newRequest.url())) {
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return;
        }
    }

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterWillSendRequest(newRequest, redirectResponse))
        return;
#endif

    setRequest(newRequest);

    if (!redirectResponse.isNull()) {
        // We checked application cache for initial URL, now we need to check it for redirected one.
        ASSERT(!m_substituteData.isValid());
        m_applicationCacheHost->maybeLoadMainResourceForRedirect(newRequest, m_substituteData);
        if (m_substituteData.isValid()) {
            RELEASE_ASSERT(m_mainResource);
            ResourceLoader* loader = m_mainResource->loader();
            m_identifierForLoadWithoutResourceLoader = loader ? loader->identifier() : m_mainResource->identifierForLoadWithoutResourceLoader();
        }
    }

    // FIXME: Ideally we'd stop the I/O until we hear back from the navigation policy delegate
    // listener. But there's no way to do that in practice. So instead we cancel later if the
    // listener tells us to. In practice that means the navigation policy needs to be decided
    // synchronously for these redirect cases.
    if (redirectResponse.isNull())
        return;

    ASSERT(!m_waitingForNavigationPolicy);
    m_waitingForNavigationPolicy = true;
    frameLoader()->policyChecker().checkNavigationPolicy(newRequest, [this](const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue) {
        continueAfterNavigationPolicy(request, shouldContinue);
    });
}

void DocumentLoader::continueAfterNavigationPolicy(const ResourceRequest&, bool shouldContinue)
{
    ASSERT(m_waitingForNavigationPolicy);
    m_waitingForNavigationPolicy = false;
    if (!shouldContinue)
        stopLoadingForPolicyChange();
    else if (m_substituteData.isValid()) {
        // A redirect resulted in loading substitute data.
        ASSERT(timing().redirectCount());

        // We need to remove our reference to the CachedResource in favor of a SubstituteData load.
        // This will probably trigger the cancellation of the CachedResource's underlying ResourceLoader, though there is a
        // small chance that the resource is being loaded by a different Frame, preventing the ResourceLoader from being cancelled.
        // If the ResourceLoader is indeed cancelled, it would normally send resource load callbacks.
        // However, from an API perspective, this isn't a cancellation. Therefore, sever our relationship with the network load,
        // but prevent the ResourceLoader from sending ResourceLoadNotifier callbacks.
        RefPtr<ResourceLoader> resourceLoader = mainResourceLoader();
        if (resourceLoader) {
            ASSERT(resourceLoader->shouldSendResourceLoadCallbacks());
            resourceLoader->setSendCallbackPolicy(DoNotSendCallbacks);
        }

        clearMainResource();

        if (resourceLoader)
            resourceLoader->setSendCallbackPolicy(SendCallbacks);
        handleSubstituteDataLoadSoon();
    }
}

void DocumentLoader::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterResponseReceived(resource, response))
        return;
#endif

    ASSERT_UNUSED(resource, m_mainResource == resource);
    Ref<DocumentLoader> protect(*this);
    bool willLoadFallback = m_applicationCacheHost->maybeLoadFallbackForMainResponse(request(), response);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use.
    if (willLoadFallback)
        MemoryCache::singleton().remove(*m_mainResource);

    if (willLoadFallback)
        return;

    const auto& commonHeaders = response.httpHeaderFields().commonHeaders();
    auto it = commonHeaders.find(HTTPHeaderName::XFrameOptions);
    if (it != commonHeaders.end()) {
        String content = it->value;
        ASSERT(m_identifierForLoadWithoutResourceLoader || m_mainResource);
        unsigned long identifier = m_identifierForLoadWithoutResourceLoader ? m_identifierForLoadWithoutResourceLoader : m_mainResource->identifier();
        ASSERT(identifier);
        if (frameLoader()->shouldInterruptLoadForXFrameOptions(content, response.url(), identifier)) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame, *this, identifier, response);
            String message = "Refused to display '" + response.url().stringCenterEllipsizedToLength() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
            frame()->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, identifier);
            frame()->document()->enforceSandboxFlags(SandboxOrigin);
            if (HTMLFrameOwnerElement* ownerElement = frame()->ownerElement())
                ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, false, false));

            // The load event might have detached this frame. In that case, the load will already have been cancelled during detach.
            if (frameLoader())
                cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
            return;
        }
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    if (m_isLoadingMultipartContent) {
        setupForReplace();
        m_mainResource->clear();
    } else if (response.isMultipart())
        m_isLoadingMultipartContent = true;

    m_response = response;

    if (m_identifierForLoadWithoutResourceLoader) {
        if (m_mainResource && m_mainResource->wasRedirected()) {
            ASSERT(m_mainResource->status() == CachedResource::Status::Cached);
            frameLoader()->client().dispatchDidReceiveServerRedirectForProvisionalLoad();
        }
        addResponse(m_response);
        frameLoader()->notifier().dispatchDidReceiveResponse(this, m_identifierForLoadWithoutResourceLoader, m_response, 0);
    }

    ASSERT(!m_waitingForContentPolicy);
    ASSERT(frameLoader());
    m_waitingForContentPolicy = true;

    // Always show content with valid substitute data.
    if (m_substituteData.isValid()) {
        continueAfterContentPolicy(PolicyUse);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    if (m_frame->settings().forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory") {
        continueAfterContentPolicy(PolicyUse);
        return;
    }
#endif

    if (m_response.isHttpVersion0_9()) {
        ASSERT(m_identifierForLoadWithoutResourceLoader || m_mainResource);
        unsigned long identifier = m_identifierForLoadWithoutResourceLoader ? m_identifierForLoadWithoutResourceLoader : m_mainResource->identifier();
        String message = "Sandboxing '" + response.url().string() + "' because it is using HTTP/0.9.";
        m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, identifier);
        frameLoader()->forceSandboxFlags(SandboxScripts | SandboxPlugins);
    }

    frameLoader()->policyChecker().checkContentPolicy(m_response, [this](PolicyAction policy) {
        continueAfterContentPolicy(policy);
    });
}

void DocumentLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (isStopping())
        return;

    URL url = m_request.url();
    const String& mimeType = m_response.mimeType();
    
    switch (policy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemoteWebArchive = (equalLettersIgnoringASCIICase(mimeType, "application/x-webarchive")
            || equalLettersIgnoringASCIICase(mimeType, "application/x-mimearchive")
#if PLATFORM(GTK)
            || equalLettersIgnoringASCIICase(mimeType, "message/rfc822")
#endif
            || equalLettersIgnoringASCIICase(mimeType, "multipart/related"))
            && !m_substituteData.isValid() && !SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol());
        if (!frameLoader()->client().canShowMIMEType(mimeType) || isRemoteWebArchive) {
            frameLoader()->policyChecker().cannotShowMIMEType(m_response);
            // Check reachedTerminalState since the load may have already been canceled inside of _handleUnimplementablePolicyWithErrorCode::.
            stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyDownload: {
        // m_mainResource can be null, e.g. when loading a substitute resource from application cache.
        if (!m_mainResource) {
            mainReceivedError(frameLoader()->client().cannotShowURLError(m_request));
            return;
        }

        if (ResourceLoader* mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyDownload(m_frame, *this, mainResourceLoader->identifier(), m_response);

        // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
        // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
        frameLoader()->setOriginalURLForDownloadRequest(m_request);

        SessionID sessionID = SessionID::defaultSessionID();
        if (frame() && frame()->page())
            sessionID = frame()->page()->sessionID();

        if (m_request.url().protocolIsData()) {
            // We decode data URL internally, there is no resource load to convert.
            frameLoader()->client().startDownload(m_request);
        } else
            frameLoader()->client().convertMainResourceLoadToDownload(this, sessionID, m_request, m_response);

        // It might have gone missing
        if (mainResourceLoader())
            static_cast<ResourceLoader*>(mainResourceLoader())->didFail(interruptedForPolicyChangeError());
        return;
    }
    case PolicyIgnore:
        if (ResourceLoader* mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyIgnore(m_frame, *this, mainResourceLoader->identifier(), m_response);
        stopLoadingForPolicyChange();
        return;
    }

    if (m_response.isHTTP()) {
        int status = m_response.httpStatusCode(); // Status may be zero when loading substitute data, in particular from a WebArchive.
        if (status && (status < 200 || status >= 300)) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
        }
    }

    if (!isStopping() && m_substituteData.isValid() && isLoadingMainResource()) {
        if (m_substituteData.content()->size())
            dataReceived(0, m_substituteData.content()->data(), m_substituteData.content()->size());
        if (isLoadingMainResource())
            finishedLoading(0);
    }
}

void DocumentLoader::commitLoad(const char* data, int length)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<Frame> protectFrame(m_frame);
    Ref<DocumentLoader> protectLoader(*this);

    commitIfReady();
    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    if (!frameLoader)
        return;
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (ArchiveFactory::isArchiveMimeType(response().mimeType()))
        return;
#endif
    frameLoader->client().committedLoad(this, data, length);

    if (isMultipartReplacingLoad())
        frameLoader->client().didReplaceMultipartContent();
}

ResourceError DocumentLoader::interruptedForPolicyChangeError() const
{
    return frameLoader()->client().interruptedForPolicyChangeError(request());
}

void DocumentLoader::stopLoadingForPolicyChange()
{
    ResourceError error = interruptedForPolicyChangeError();
    error.setIsCancellation(true);
    cancelMainResourceLoad(error);
}

void DocumentLoader::commitData(const char* bytes, size_t length)
{
    if (!m_gotFirstByte) {
        m_gotFirstByte = true;
        m_writer.begin(documentURL(), false);
        m_writer.setDocumentWasLoadedAsPartOfNavigation();

        if (SecurityPolicy::allowSubstituteDataAccessToLocal() && m_originalSubstituteDataWasValid) {
            // If this document was loaded with substituteData, then the document can
            // load local resources. See https://bugs.webkit.org/show_bug.cgi?id=16756
            // and https://bugs.webkit.org/show_bug.cgi?id=19760 for further
            // discussion.
            m_frame->document()->securityOrigin()->grantLoadLocalResources();
        }

        if (frameLoader()->stateMachine().creatingInitialEmptyDocument())
            return;
        
#if ENABLE(MHTML)
        // The origin is the MHTML file, we need to set the base URL to the document encoded in the MHTML so
        // relative URLs are resolved properly.
        if (m_archive && m_archive->type() == Archive::MHTML)
            m_frame->document()->setBaseURLOverride(m_archive->mainResource()->url());
#endif

        // Call receivedFirstData() exactly once per load. We should only reach this point multiple times
        // for multipart loads, and FrameLoader::isReplacing() will be true after the first time.
        if (!isMultipartReplacingLoad())
            frameLoader()->receivedFirstData();

        // The load could be canceled under receivedFirstData(), which makes delegate calls and even sometimes dispatches DOM events.
        if (!isLoading())
            return;

        bool userChosen;
        String encoding;
        if (overrideEncoding().isNull()) {
            userChosen = false;
            encoding = response().textEncodingName();
#if ENABLE(WEB_ARCHIVE)
            if (m_archive && m_archive->type() == Archive::WebArchive)
                encoding = m_archive->mainResource()->textEncoding();
#endif
        } else {
            userChosen = true;
            encoding = overrideEncoding();
        }

        m_writer.setEncoding(encoding, userChosen);
    }

#if ENABLE(CONTENT_EXTENSIONS)
    auto& extensionStyleSheets = m_frame->document()->extensionStyleSheets();

    for (auto& pendingStyleSheet : m_pendingNamedContentExtensionStyleSheets)
        extensionStyleSheets.maybeAddContentExtensionSheet(pendingStyleSheet.key, *pendingStyleSheet.value);
    for (auto& pendingSelectorEntry : m_pendingContentExtensionDisplayNoneSelectors) {
        for (const auto& pendingSelector : pendingSelectorEntry.value)
            extensionStyleSheets.addDisplayNoneSelector(pendingSelectorEntry.key, pendingSelector.first, pendingSelector.second);
    }

    m_pendingNamedContentExtensionStyleSheets.clear();
    m_pendingContentExtensionDisplayNoneSelectors.clear();
#endif

    ASSERT(m_frame->document()->parsing());
    m_writer.addData(bytes, length);
}

void DocumentLoader::dataReceived(CachedResource* resource, const char* data, int length)
{
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterDataReceived(resource, data, length))
        return;
#endif

    ASSERT(data);
    ASSERT(length);
    ASSERT_UNUSED(resource, resource == m_mainResource);
    ASSERT(!m_response.isNull());

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    if (m_identifierForLoadWithoutResourceLoader)
        frameLoader()->notifier().dispatchDidReceiveData(this, m_identifierForLoadWithoutResourceLoader, data, length, -1);

    m_applicationCacheHost->mainResourceDataReceived(data, length, -1, false);
    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    if (!isMultipartReplacingLoad())
        commitLoad(data, length);
}

void DocumentLoader::setupForReplace()
{
    if (!mainResourceData())
        return;

    frameLoader()->client().willReplaceMultipartContent();
    
    maybeFinishLoadingMultipartContent();
    maybeCreateArchive();
    m_writer.end();
    frameLoader()->setReplacing();
    m_gotFirstByte = false;
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    clearArchiveResources();
#endif
}

void DocumentLoader::checkLoadComplete()
{
    if (!m_frame || isLoading())
        return;

    ASSERT(this == frameLoader()->activeDocumentLoader());
    m_frame->document()->domWindow()->finishedLoading();
}

void DocumentLoader::attachToFrame(Frame& frame)
{
    if (m_frame == &frame)
        return;

    ASSERT(!m_frame);
    m_frame = &frame;
    m_writer.setFrame(&frame);
    attachToFrame();

#ifndef NDEBUG
    m_hasEverBeenAttached = true;
#endif
}

void DocumentLoader::attachToFrame()
{
    ASSERT(m_frame);
}

void DocumentLoader::detachFromFrame()
{
#ifndef NDEBUG
    if (m_hasEverBeenAttached)
        ASSERT_WITH_MESSAGE(m_frame, "detachFromFrame() is being called on a DocumentLoader twice without an attachToFrame() inbetween");
    else
        ASSERT_WITH_MESSAGE(m_frame, "detachFromFrame() is being called on a DocumentLoader that has never attached to any Frame");
#endif
    RefPtr<Frame> protectFrame(m_frame);
    Ref<DocumentLoader> protectLoader(*this);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so kill all the loads.
    stopLoading();
    if (m_mainResource && m_mainResource->hasClient(this))
        m_mainResource->removeClient(this);
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    m_applicationCacheHost->setDOMApplicationCache(nullptr);

    cancelPolicyCheckIfNeeded();

    // Even though we ASSERT at the top of this method that we have an m_frame, we're seeing crashes where m_frame is null.
    // This means either that a DocumentLoader is detaching twice, or is detaching before ever having attached.
    // Until we figure out how that is happening, null check m_frame before dereferencing it here.
    // <rdar://problem/21293082> and https://bugs.webkit.org/show_bug.cgi?id=146786
    if (m_frame)
        InspectorInstrumentation::loaderDetachedFromFrame(*m_frame, *this);

    m_frame = nullptr;
}

void DocumentLoader::clearMainResourceLoader()
{
    m_loadingMainResource = false;

    if (this == frameLoader()->activeDocumentLoader())
        checkLoadComplete();
}

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameStateComplete) {
        if (m_frame->settings().needsIsLoadingInAPISenseQuirk() && !m_subresourceLoaders.isEmpty())
            return true;
    
        Document* doc = m_frame->document();
        if ((isLoadingMainResource() || !m_frame->document()->loadEventFinished()) && isLoading())
            return true;
        if (m_cachedResourceLoader->requestCount())
            return true;
        if (doc->processingLoadEvent())
            return true;
        if (doc->hasActiveParser())
            return true;
    }
    return frameLoader()->subframeIsLoading();
}

bool DocumentLoader::maybeCreateArchive()
{
#if !ENABLE(WEB_ARCHIVE) && !ENABLE(MHTML)
    return false;
#else
    
    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    m_archive = ArchiveFactory::create(m_response.url(), mainResourceData().get(), m_response.mimeType());
    if (!m_archive)
        return false;
    
    addAllArchiveResources(m_archive.get());
    ArchiveResource* mainResource = m_archive->mainResource();
    m_parsedArchiveData = mainResource->data();
    m_writer.setMIMEType(mainResource->mimeType());
    
    ASSERT(m_frame->document());
    commitData(mainResource->data()->data(), mainResource->data()->size());
    return true;
#endif // !ENABLE(WEB_ARCHIVE) && !ENABLE(MHTML)
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

void DocumentLoader::setArchive(PassRefPtr<Archive> archive)
{
    m_archive = archive;
    addAllArchiveResources(m_archive.get());
}

void DocumentLoader::addAllArchiveResources(Archive* archive)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = std::make_unique<ArchiveResourceCollection>();
        
    ASSERT(archive);
    if (!archive)
        return;
        
    m_archiveResourceCollection->addAllResources(archive);
}

// FIXME: Adding a resource directly to a DocumentLoader/ArchiveResourceCollection seems like bad design, but is API some apps rely on.
// Can we change the design in a manner that will let us deprecate that API without reducing functionality of those apps?
void DocumentLoader::addArchiveResource(PassRefPtr<ArchiveResource> resource)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = std::make_unique<ArchiveResourceCollection>();
        
    ASSERT(resource);
    if (!resource)
        return;
        
    m_archiveResourceCollection->addResource(resource);
}

PassRefPtr<Archive> DocumentLoader::popArchiveForSubframe(const String& frameName, const URL& url)
{
    return m_archiveResourceCollection ? m_archiveResourceCollection->popSubframeArchive(frameName, url) : PassRefPtr<Archive>(nullptr);
}

void DocumentLoader::clearArchiveResources()
{
    m_archiveResourceCollection = nullptr;
    m_substituteResourceDeliveryTimer.stop();
}

SharedBuffer* DocumentLoader::parsedArchiveData() const
{
    return m_parsedArchiveData.get();
}

#endif // ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

ArchiveResource* DocumentLoader::archiveResourceForURL(const URL& url) const
{
    if (!m_archiveResourceCollection)
        return nullptr;
    ArchiveResource* resource = m_archiveResourceCollection->archiveResourceForURL(url);
    if (!resource || resource->shouldIgnoreWhenUnarchiving())
        return nullptr;
    return resource;
}

PassRefPtr<ArchiveResource> DocumentLoader::mainResource() const
{
    RefPtr<SharedBuffer> data = mainResourceData();
    if (!data)
        data = SharedBuffer::create();
        
    auto& response = this->response();
    return ArchiveResource::create(data, response.url(), response.mimeType(), response.textEncodingName(), frame()->tree().uniqueName());
}

PassRefPtr<ArchiveResource> DocumentLoader::subresource(const URL& url) const
{
    if (!isCommitted())
        return nullptr;
    
    CachedResource* resource = m_cachedResourceLoader->cachedResource(url);
    if (!resource || !resource->isLoaded())
        return archiveResourceForURL(url);

    if (resource->type() == CachedResource::MainResource)
        return nullptr;

    auto* data = resource->resourceBuffer();
    if (!data)
        return nullptr;

    return ArchiveResource::create(data, url, resource->response());
}

Vector<RefPtr<ArchiveResource>> DocumentLoader::subresources() const
{
    if (!isCommitted())
        return { };

    Vector<RefPtr<ArchiveResource>> subresources;

    for (auto& cachedResourceHandle : m_cachedResourceLoader->allCachedResources().values()) {
        if (RefPtr<ArchiveResource> subresource = this->subresource(URL(ParsedURLString, cachedResourceHandle->url())))
            subresources.append(WTFMove(subresource));
    }

    return subresources;
}

void DocumentLoader::deliverSubstituteResourcesAfterDelay()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;
    if (!m_substituteResourceDeliveryTimer.isActive())
        m_substituteResourceDeliveryTimer.startOneShot(0);
}

void DocumentLoader::substituteResourceDeliveryTimerFired()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame && m_frame->page());
    if (m_frame->page()->defersLoading())
        return;

    SubstituteResourceMap copy;
    copy.swap(m_pendingSubstituteResources);

    for (auto& entry : copy) {
        auto& loader = entry.key;
        SubstituteResource* resource = entry.value.get();

        if (resource)
            resource->deliver(*loader);
        else {
            // A null resource means that we should fail the load.
            // FIXME: Maybe we should use another error here - something like "not in cache".
            loader->didFail(loader->cannotShowURLError());
        }
    }
}

#ifndef NDEBUG
bool DocumentLoader::isSubstituteLoadPending(ResourceLoader* loader) const
{
    return m_pendingSubstituteResources.contains(loader);
}
#endif

void DocumentLoader::cancelPendingSubstituteLoad(ResourceLoader* loader)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    m_pendingSubstituteResources.remove(loader);
    if (m_pendingSubstituteResources.isEmpty())
        m_substituteResourceDeliveryTimer.stop();
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
bool DocumentLoader::scheduleArchiveLoad(ResourceLoader* loader, const ResourceRequest& request)
{
    if (ArchiveResource* resource = archiveResourceForURL(request.url())) {
        scheduleSubstituteResourceLoad(*loader, *resource);
        return true;
    }

    if (!m_archive)
        return false;

    switch (m_archive->type()) {
#if ENABLE(WEB_ARCHIVE)
    case Archive::WebArchive:
        // WebArchiveDebugMode means we fail loads instead of trying to fetch them from the network if they're not in the archive.
        return m_frame->settings().webArchiveDebugModeEnabled() && ArchiveFactory::isArchiveMimeType(responseMIMEType());
#endif
#if ENABLE(MHTML)
    case Archive::MHTML:
        return true; // Always fail the load for resources not included in the MHTML.
#endif
    default:
        return false;
    }
}
#endif // ENABLE(WEB_ARCHIVE)

void DocumentLoader::scheduleSubstituteResourceLoad(ResourceLoader& loader, SubstituteResource& resource)
{
    m_pendingSubstituteResources.set(&loader, &resource);
    deliverSubstituteResourcesAfterDelay();
}

void DocumentLoader::addResponse(const ResourceResponse& r)
{
    if (!m_stopRecordingResponses)
        m_responses.append(r);
}

void DocumentLoader::stopRecordingResponses()
{
    m_stopRecordingResponses = true;
    m_responses.shrinkToFit();
}

void DocumentLoader::setTitle(const StringWithDirection& title)
{
    if (m_pageTitle == title)
        return;

    frameLoader()->willChangeTitle(this);
    m_pageTitle = title;
    frameLoader()->didChangeTitle(this);
}

URL DocumentLoader::urlForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates
    // for unreachable URLs, because these can't be stored in history.
    if (m_substituteData.isValid() && !m_substituteData.shouldRevealToSessionHistory())
        return unreachableURL();

    return m_originalRequestCopy.url();
}

bool DocumentLoader::urlForHistoryReflectsFailure() const
{
    return m_substituteData.isValid() || m_response.httpStatusCode() >= 400;
}

const URL& DocumentLoader::originalURL() const
{
    return m_originalRequestCopy.url();
}

const URL& DocumentLoader::responseURL() const
{
    return m_response.url();
}

URL DocumentLoader::documentURL() const
{
    URL url = substituteData().response().url();
#if ENABLE(WEB_ARCHIVE)
    if (url.isEmpty() && m_archive && m_archive->type() == Archive::WebArchive)
        url = m_archive->mainResource()->url();
#endif
    if (url.isEmpty())
        url = m_request.url();
    if (url.isEmpty())
        url = m_response.url();
    return url;
}

const String& DocumentLoader::responseMIMEType() const
{
    return m_response.mimeType();
}

const String& DocumentLoader::currentContentType() const
{
    return m_writer.mimeType();
}

#if PLATFORM(IOS)
// FIXME: This method seems to violate the encapsulation of this class.
void DocumentLoader::setResponseMIMEType(const String& responseMimeType)
{
    m_response.setMimeType(responseMimeType);
}
#endif

const URL& DocumentLoader::unreachableURL() const
{
    return m_substituteData.failingURL();
}

void DocumentLoader::setDefersLoading(bool defers)
{
    // Multiple frames may be loading the same main resource simultaneously. If deferral state changes,
    // each frame's DocumentLoader will try to send a setDefersLoading() to the same underlying ResourceLoader. Ensure only
    // the "owning" DocumentLoader does so, as setDefersLoading() is not resilient to setting the same value repeatedly.
    if (mainResourceLoader() && mainResourceLoader()->documentLoader() == this)
        mainResourceLoader()->setDefersLoading(defers);

    setAllDefersLoading(m_subresourceLoaders, defers);
    setAllDefersLoading(m_plugInStreamLoaders, defers);
    if (!defers)
        deliverSubstituteResourcesAfterDelay();
}

void DocumentLoader::setMainResourceDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    if (m_mainResource)
        m_mainResource->setDataBufferingPolicy(dataBufferingPolicy);
}

void DocumentLoader::stopLoadingPlugIns()
{
    cancelAll(m_plugInStreamLoaders);
}

void DocumentLoader::stopLoadingSubresources()
{
    cancelAll(m_subresourceLoaders);
    ASSERT(m_subresourceLoaders.isEmpty());
}

void DocumentLoader::addSubresourceLoader(ResourceLoader* loader)
{
    // The main resource's underlying ResourceLoader will ask to be added here.
    // It is much simpler to handle special casing of main resource loads if we don't
    // let it be added. In the main resource load case, mainResourceLoader()
    // will still be null at this point, but m_gotFirstByte should be false here if and only
    // if we are just starting the main resource load.
    if (!m_gotFirstByte)
        return;
    ASSERT(loader->identifier());
    ASSERT(!m_subresourceLoaders.contains(loader->identifier()));
    ASSERT(!mainResourceLoader() || mainResourceLoader() != loader);

    // A page in the PageCache should not be able to start loads.
    ASSERT_WITH_SECURITY_IMPLICATION(!document() || !document()->inPageCache());

    m_subresourceLoaders.add(loader->identifier(), loader);
}

void DocumentLoader::removeSubresourceLoader(ResourceLoader* loader)
{
    ASSERT(loader->identifier());

    if (!m_subresourceLoaders.remove(loader->identifier()))
        return;
    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader().checkLoadComplete();
}

void DocumentLoader::addPlugInStreamLoader(ResourceLoader& loader)
{
    ASSERT(loader.identifier());
    ASSERT(!m_plugInStreamLoaders.contains(loader.identifier()));

    m_plugInStreamLoaders.add(loader.identifier(), &loader);
}

void DocumentLoader::removePlugInStreamLoader(ResourceLoader& loader)
{
    ASSERT(loader.identifier());
    ASSERT(&loader == m_plugInStreamLoaders.get(loader.identifier()));

    m_plugInStreamLoaders.remove(loader.identifier());
    checkLoadComplete();
}

bool DocumentLoader::isMultipartReplacingLoad() const
{
    return isLoadingMultipartContent() && frameLoader()->isReplacing();
}

bool DocumentLoader::maybeLoadEmpty()
{
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol()));
    if (!shouldLoadEmpty && !frameLoader()->client().representationExistsForURLScheme(m_request.url().protocol()))
        return false;

    if (m_request.url().isEmpty() && !frameLoader()->stateMachine().creatingInitialEmptyDocument()) {
        m_request.setURL(blankURL());
        if (isLoadingMainResource())
            frameLoader()->client().dispatchDidChangeProvisionalURL();
    }

    String mimeType = shouldLoadEmpty ? "text/html" : frameLoader()->client().generatedMIMETypeForURLScheme(m_request.url().protocol());
    m_response = ResourceResponse(m_request.url(), mimeType, 0, String());
    finishedLoading(monotonicallyIncreasingTime());
    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    m_mainDocumentError = ResourceError();
    timing().markNavigationStart();
    ASSERT(!m_mainResource);
    ASSERT(!m_loadingMainResource);
    m_loadingMainResource = true;

    if (maybeLoadEmpty())
        return;

#if ENABLE(CONTENT_FILTERING)
    m_contentFilter = !m_substituteData.isValid() ? ContentFilter::create(*this) : nullptr;
#endif

    // FIXME: Is there any way the extra fields could have not been added by now?
    // If not, it would be great to remove this line of code.
    // Note that currently, some requests may have incorrect extra fields even if this function has been called,
    // because we pass a wrong loadType (see FIXME in addExtraFieldsToMainResourceRequest()).
    frameLoader()->addExtraFieldsToMainResourceRequest(m_request);

    ASSERT(timing().navigationStart());
    ASSERT(!timing().fetchStart());
    timing().markFetchStart();
    willSendRequest(m_request, ResourceResponse());

    // willSendRequest() may lead to our Frame being detached or cancelling the load via nulling the ResourceRequest.
    if (!m_frame || m_request.isNull())
        return;

    m_applicationCacheHost->maybeLoadMainResource(m_request, m_substituteData);

    if (m_substituteData.isValid() && m_frame->page()) {
        m_identifierForLoadWithoutResourceLoader = m_frame->page()->progress().createUniqueIdentifier();
        frameLoader()->notifier().assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, this, m_request);
        frameLoader()->notifier().dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, m_request, ResourceResponse());
        handleSubstituteDataLoadSoon();
        return;
    }

    ResourceRequest request(m_request);
    request.setRequester(ResourceRequest::Requester::Main);
    // If this is a reload the cache layer might have made the previous request conditional. DocumentLoader can't handle 304 responses itself.
    request.makeUnconditional();

    static NeverDestroyed<ResourceLoaderOptions> mainResourceLoadOptions(SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForAllCredentials, ClientRequestedCredentials, SkipSecurityCheck, UseDefaultOriginRestrictionsForType, IncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading);
    CachedResourceRequest cachedResourceRequest(request, mainResourceLoadOptions);
    cachedResourceRequest.setInitiator(*this);
    m_mainResource = m_cachedResourceLoader->requestMainResource(cachedResourceRequest);

#if ENABLE(CONTENT_EXTENSIONS)
    if (m_mainResource && m_mainResource->errorOccurred() && m_frame->page() && m_mainResource->resourceError().domain() == ContentExtensions::WebKitContentBlockerDomain) {
        m_identifierForLoadWithoutResourceLoader = m_frame->page()->progress().createUniqueIdentifier();
        frameLoader()->notifier().assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, this, request);
        frameLoader()->notifier().dispatchDidFailLoading(this, m_identifierForLoadWithoutResourceLoader, frameLoader()->blockedByContentBlockerError(m_request));
        m_mainResource = nullptr;
    }
#endif

    if (!m_mainResource) {
        setRequest(ResourceRequest());
        // If the load was aborted by clearing m_request, it's possible the ApplicationCacheHost
        // is now in a state where starting an empty load will be inconsistent. Replace it with
        // a new ApplicationCacheHost.
        m_applicationCacheHost = std::make_unique<ApplicationCacheHost>(*this);
        maybeLoadEmpty();
        return;
    }

    if (!mainResourceLoader()) {
        m_identifierForLoadWithoutResourceLoader = m_frame->page()->progress().createUniqueIdentifier();
        frameLoader()->notifier().assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, this, request);
        frameLoader()->notifier().dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, request, ResourceResponse());
    }

    becomeMainResourceClient();

    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    if (mainResourceLoader())
        request = mainResourceLoader()->originalRequest();
    // If there was a fragment identifier on m_request, the cache will have stripped it. m_request should include
    // the fragment identifier, so add that back in.
    if (equalIgnoringFragmentIdentifier(m_request.url(), request.url()))
        request.setURL(m_request.url());
    setRequest(request);
}

void DocumentLoader::cancelPolicyCheckIfNeeded()
{
    RELEASE_ASSERT(frameLoader());

    if (m_waitingForContentPolicy || m_waitingForNavigationPolicy) {
        frameLoader()->policyChecker().cancelCheck();
        m_waitingForContentPolicy = false;
        m_waitingForNavigationPolicy = false;
    }
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& resourceError)
{
    Ref<DocumentLoader> protect(*this);
    ResourceError error = resourceError.isNull() ? frameLoader()->cancelledError(m_request) : resourceError;

    m_dataLoadTimer.stop();

    cancelPolicyCheckIfNeeded();

    if (mainResourceLoader())
        mainResourceLoader()->cancel(error);

    clearMainResource();

    mainReceivedError(error);
}

void DocumentLoader::clearMainResource()
{
    if (m_mainResource && m_mainResource->hasClient(this))
        m_mainResource->removeClient(this);
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    m_mainResource = nullptr;
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader* loader)
{
    unsigned long identifier = loader->identifier();
    ASSERT(identifier);

    if (!m_multipartSubresourceLoaders.add(identifier, loader).isNewEntry) {
        ASSERT(m_multipartSubresourceLoaders.get(identifier) == loader);
        ASSERT(!m_subresourceLoaders.contains(identifier));
    } else {
        ASSERT(m_subresourceLoaders.contains(identifier));
        m_subresourceLoaders.remove(identifier);
    }

    checkLoadComplete();
    if (Frame* frame = m_frame)
        frame->loader().checkLoadComplete();    
}

void DocumentLoader::maybeFinishLoadingMultipartContent()
{
    if (!isMultipartReplacingLoad())
        return;

    frameLoader()->setupForReplace();
    m_committed = false;
    RefPtr<SharedBuffer> resourceData = mainResourceData();
    commitLoad(resourceData->data(), resourceData->size());
}

void DocumentLoader::iconLoadDecisionAvailable()
{
    if (m_frame)
        m_frame->loader().icon().loadDecisionReceived(iconDatabase().synchronousLoadDecisionForIconURL(frameLoader()->icon().url(), this));
}

static void iconLoadDecisionCallback(IconLoadDecision decision, void* context)
{
    static_cast<DocumentLoader*>(context)->continueIconLoadWithDecision(decision);
}

void DocumentLoader::getIconLoadDecisionForIconURL(const String& urlString)
{
    if (m_iconLoadDecisionCallback)
        m_iconLoadDecisionCallback->invalidate();
    m_iconLoadDecisionCallback = IconLoadDecisionCallback::create(this, iconLoadDecisionCallback);
    iconDatabase().loadDecisionForIconURL(urlString, m_iconLoadDecisionCallback);
}

void DocumentLoader::continueIconLoadWithDecision(IconLoadDecision decision)
{
    ASSERT(m_iconLoadDecisionCallback);
    m_iconLoadDecisionCallback = nullptr;
    if (m_frame)
        m_frame->loader().icon().continueLoadWithDecision(decision);
}

static void iconDataCallback(SharedBuffer*, void*)
{
    // FIXME: Implement this once we know what parts of WebCore actually need the icon data returned.
}

void DocumentLoader::getIconDataForIconURL(const String& urlString)
{   
    if (m_iconDataCallback)
        m_iconDataCallback->invalidate();
    m_iconDataCallback = IconDataCallback::create(this, iconDataCallback);
    iconDatabase().iconDataForIconURL(urlString, m_iconDataCallback);
}

void DocumentLoader::dispatchOnloadEvents()
{
    m_wasOnloadDispatched = true;
    applicationCacheHost()->stopDeferringEvents();
}

void DocumentLoader::setTriggeringAction(const NavigationAction& action)
{
    m_triggeringAction = action.copyWithShouldOpenExternalURLsPolicy(m_frame ? shouldOpenExternalURLsPolicyToPropagate() : m_shouldOpenExternalURLsPolicy);
}

ShouldOpenExternalURLsPolicy DocumentLoader::shouldOpenExternalURLsPolicyToPropagate() const
{
    if (!m_frame || !m_frame->isMainFrame())
        return ShouldOpenExternalURLsPolicy::ShouldNotAllow;

    return m_shouldOpenExternalURLsPolicy;
}

void DocumentLoader::becomeMainResourceClient()
{
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter)
        m_contentFilter->startFilteringMainResource(*m_mainResource);
#endif
    m_mainResource->addClient(this);
}

#if ENABLE(CONTENT_EXTENSIONS)
void DocumentLoader::addPendingContentExtensionSheet(const String& identifier, StyleSheetContents& sheet)
{
    ASSERT(!m_gotFirstByte);
    m_pendingNamedContentExtensionStyleSheets.set(identifier, &sheet);
}

void DocumentLoader::addPendingContentExtensionDisplayNoneSelector(const String& identifier, const String& selector, uint32_t selectorID)
{
    ASSERT(!m_gotFirstByte);
    auto addResult = m_pendingContentExtensionDisplayNoneSelectors.add(identifier, Vector<std::pair<String, uint32_t>>());
    addResult.iterator->value.append(std::make_pair(selector, selectorID));
}
#endif

#if ENABLE(CONTENT_FILTERING)
void DocumentLoader::installContentFilterUnblockHandler(ContentFilter& contentFilter)
{
    ContentFilterUnblockHandler unblockHandler { contentFilter.unblockHandler() };
    unblockHandler.setUnreachableURL(documentURL());
    RefPtr<Frame> frame { this->frame() };
    String unblockRequestDeniedScript { contentFilter.unblockRequestDeniedScript() };
    if (!unblockRequestDeniedScript.isEmpty() && frame) {
        static_assert(std::is_base_of<ThreadSafeRefCounted<Frame>, Frame>::value, "Frame must be ThreadSafeRefCounted.");
        StringCapture capturedScript { unblockRequestDeniedScript };
        unblockHandler.wrapWithDecisionHandler([frame, capturedScript](bool unblocked) {
            if (!unblocked)
                frame->script().executeScript(capturedScript.string());
        });
    }
    frameLoader()->client().contentFilterDidBlockLoad(WTFMove(unblockHandler));
}

void DocumentLoader::contentFilterDidBlock()
{
    ASSERT(m_contentFilter);

    installContentFilterUnblockHandler(*m_contentFilter);

    URL blockedURL;
    blockedURL.setProtocol(ContentFilter::urlScheme());
    blockedURL.setHost(ASCIILiteral("blocked-page"));
    auto replacementData = m_contentFilter->replacementData();
    ResourceResponse response(URL(), ASCIILiteral("text/html"), replacementData->size(), ASCIILiteral("UTF-8"));
    SubstituteData substituteData { adoptRef(&replacementData.leakRef()), documentURL(), response, SubstituteData::SessionHistoryVisibility::Hidden };
    frame()->navigationScheduler().scheduleSubstituteDataLoad(blockedURL, substituteData);
}
#endif


} // namespace WebCore
