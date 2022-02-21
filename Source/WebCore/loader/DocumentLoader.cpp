/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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
#include "Archive.h"
#include "ArchiveResourceCollection.h"
#include "CachedPage.h"
#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "ContentExtensionError.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginOpenerPolicy.h"
#include "CustomHeaderFields.h"
#include "DOMWindow.h"
#include "DocumentInlines.h"
#include "DocumentParser.h"
#include "DocumentWriter.h"
#include "ElementChildIterator.h"
#include "Event.h"
#include "EventNames.h"
#include "ExtensionStyleSheets.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLObjectElement.h"
#include "HTTPHeaderNames.h"
#include "HistoryItem.h"
#include "HistoryController.h"
#include "IconLoader.h"
#include "InspectorInstrumentation.h"
#include "LegacySchemeRegistry.h"
#include "LinkIconCollector.h"
#include "LinkIconType.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "MixedContentChecker.h"
#include "NavigationRequester.h"
#include "NavigationScheduler.h"
#include "NetworkLoadMetrics.h"
#include "NetworkStorageSession.h"
#include "Page.h"
#include "Performance.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "Quirks.h"
#include "ResourceHandle.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "SWClientConnection.h"
#include "ScriptableDocumentParser.h"
#include "SecurityPolicy.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerProvider.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include "TextResourceDecoder.h"
#include "UserContentProvider.h"
#include "UserContentURLPattern.h"
#include <wtf/Assertions.h>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifestLoader.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#endif

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
#include "ArchiveFactory.h"
#endif

#if ENABLE(CONTENT_FILTERING)
#include "ContentFilter.h"
#include "FrameLoadRequest.h"
#include "ScriptController.h"
#endif

#if USE(QUICK_LOOK)
#include "PreviewConverter.h"
#include "QuickLook.h"
#endif

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
#include "NetworkStorageSession.h"
#endif

#define PAGE_ID ((m_frame ? valueOrDefault(m_frame->pageID()) : PageIdentifier()).toUInt64())
#define FRAME_ID ((m_frame ? valueOrDefault(m_frame->frameID()) : FrameIdentifier()).toUInt64())
#define IS_MAIN_FRAME (m_frame ? m_frame->isMainFrame() : false)
#define DOCUMENTLOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] DocumentLoader::" fmt, this, PAGE_ID, FRAME_ID, IS_MAIN_FRAME, ##__VA_ARGS__)

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/DocumentLoaderAdditions.cpp>
#endif

namespace WebCore {

static void cancelAll(const ResourceLoaderMap& loaders)
{
    for (auto& loader : copyToVector(loaders.values()))
        loader->cancel();
}

static void setAllDefersLoading(const ResourceLoaderMap& loaders, bool defers)
{
    for (auto& loader : copyToVector(loaders.values()))
        loader->setDefersLoading(defers);
}

static HashMap<ScriptExecutionContextIdentifier, DocumentLoader*>& scriptExecutionContextIdentifierToLoaderMap()
{
    static NeverDestroyed<HashMap<ScriptExecutionContextIdentifier, DocumentLoader*>> map;
    return map.get();
}

DocumentLoader* DocumentLoader::fromScriptExecutionContextIdentifier(ScriptExecutionContextIdentifier identifier)
{
    return scriptExecutionContextIdentifierToLoaderMap().get(identifier);
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DocumentLoader);

DocumentLoader::DocumentLoader(const ResourceRequest& request, const SubstituteData& substituteData)
    : FrameDestructionObserver(nullptr)
    , m_cachedResourceLoader(CachedResourceLoader::create(this))
    , m_originalRequest(request)
    , m_substituteData(substituteData)
    , m_originalRequestCopy(request)
    , m_request(request)
    , m_originalSubstituteDataWasValid(substituteData.isValid())
    , m_substituteResourceDeliveryTimer(*this, &DocumentLoader::substituteResourceDeliveryTimerFired)
    , m_applicationCacheHost(makeUnique<ApplicationCacheHost>(*this))
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
    if (!m_mainResource)
        return nullptr;
    return m_mainResource->loader();
}

DocumentLoader::~DocumentLoader()
{
    ASSERT(!m_frame || !isLoading() || frameLoader()->activeDocumentLoader() != this);
    ASSERT_WITH_MESSAGE(!m_waitingForContentPolicy, "The content policy callback should never outlive its DocumentLoader.");
    ASSERT_WITH_MESSAGE(!m_waitingForNavigationPolicy, "The navigation policy callback should never outlive its DocumentLoader.");

    m_cachedResourceLoader->clearDocumentLoader();
    clearMainResource();

#if ENABLE(SERVICE_WORKER)
    if (m_resultingClientId) {
        ASSERT(scriptExecutionContextIdentifierToLoaderMap().contains(m_resultingClientId));
        scriptExecutionContextIdentifierToLoaderMap().remove(m_resultingClientId);
    }
#endif
}

RefPtr<FragmentedSharedBuffer> DocumentLoader::mainResourceData() const
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

    bool shouldNotifyAboutProvisionalURLChange = false;
    if (handlingUnreachableURL)
        m_committed = false;
    else if (isLoadingMainResource() && req.url() != m_request.url())
        shouldNotifyAboutProvisionalURLChange = true;

    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!m_committed);

    m_request = req;
    if (shouldNotifyAboutProvisionalURLChange) {
        // Logging for <rdar://problem/54830233>.
        if (!frameLoader()->provisionalDocumentLoader())
            DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::setRequest: With no provisional document loader");
        frameLoader()->client().dispatchDidChangeProvisionalURL();
    }
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    if (!error.isNull())
        DOCUMENTLOADER_RELEASE_LOG("setMainDocumentError: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

    m_mainDocumentError = error;    
    frameLoader()->client().setMainDocumentError(this, error);
}

void DocumentLoader::mainReceivedError(const ResourceError& error)
{
    ASSERT(!error.isNull());

    if (!frameLoader())
        return;

    if (!error.isNull())
        DOCUMENTLOADER_RELEASE_LOG("mainReceivedError: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

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

    setMainDocumentError(error);
    clearMainResourceLoader();
    frameLoader()->receivedMainResourceError(error);
}

void DocumentLoader::frameDestroyed()
{
    DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::frameDestroyed: m_frame=%p", m_frame.get());
    FrameDestructionObserver::frameDestroyed();
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
void DocumentLoader::stopLoading()
{
    DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::stopLoading: m_frame=%p", m_frame.get());

    ASSERT(m_frame);
    if (!m_frame)
        return;

    RefPtr<Frame> protectedFrame(m_frame.get());
    Ref<DocumentLoader> protectedThis(*this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();

    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        Document* doc = m_frame->document();
        
        if (loading || doc->parsing())
            m_frame->loader().stopLoading(UnloadEventPolicy::None);
    }

    for (auto& callback : m_iconLoaders.values())
        callback(nullptr);
    m_iconLoaders.clear();
    m_iconsPendingLoadDecision.clear();
    
#if ENABLE(APPLICATION_MANIFEST)
    m_applicationManifestLoader = nullptr;
    m_finishedLoadingApplicationManifest = false;
    notifyFinishedLoadingApplicationManifest();
#endif

    // Always cancel multipart loaders
    cancelAll(m_multipartSubresourceLoaders);

    if (auto* document = this->document())
        document->suspendFontLoading();

    // Appcache uses ResourceHandle directly, DocumentLoader doesn't count these loads.
    m_applicationCacheHost->stopLoadingInFrame(*m_frame);
    
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

    // The frame may have been detached from this document by the onunload handler
    if (auto* frameLoader = DocumentLoader::frameLoader()) {
        DOCUMENTLOADER_RELEASE_LOG("stopLoading: canceling load");
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

void DocumentLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& metrics)
{
    ASSERT(isMainThread());
#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter && !m_contentFilter->continueAfterNotifyFinished(resource))
        return;
#endif

    if (RefPtr document = this->document()) {
        if (RefPtr domWindow = document->domWindow()) {
            if (document->settings().performanceNavigationTimingAPIEnabled())
                domWindow->performance().navigationFinished(metrics);
        }
    }

    ASSERT_UNUSED(resource, m_mainResource == &resource);
    ASSERT(m_mainResource);
    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading();
        return;
    }

    if (m_request.cachePolicy() == ResourceRequestCachePolicy::ReturnCacheDataDontLoad && !m_mainResource->wasCanceled()) {
        frameLoader()->retryAfterFailedCacheOnlyMainResourceLoad();
        return;
    }

    if (!m_mainResource->resourceError().isNull())
        DOCUMENTLOADER_RELEASE_LOG("notifyFinished: canceling load (type=%d, code=%d)", static_cast<int>(m_mainResource->resourceError().type()), m_mainResource->resourceError().errorCode());

    mainReceivedError(m_mainResource->resourceError());
}

void DocumentLoader::finishedLoading()
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!m_frame->page()->defersLoading() || frameLoader()->stateMachine().creatingInitialEmptyDocument() || InspectorInstrumentation::isDebuggerPaused(m_frame.get()));
#endif

    Ref<DocumentLoader> protectedThis(*this);

    if (m_identifierForLoadWithoutResourceLoader) {
        // A didFinishLoading delegate might try to cancel the load (despite it
        // being finished). Clear m_identifierForLoadWithoutResourceLoader
        // before calling dispatchDidFinishLoading so that we don't later try to
        // cancel the already-finished substitute load.
        NetworkLoadMetrics emptyMetrics;
        ResourceLoaderIdentifier identifier = m_identifierForLoadWithoutResourceLoader;
        m_identifierForLoadWithoutResourceLoader = { };
        frameLoader()->notifier().dispatchDidFinishLoading(this, identifier, emptyMetrics, nullptr);
    }

    maybeFinishLoadingMultipartContent();

    timing().markEndTime();
    
    commitIfReady();
    if (!frameLoader())
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_gotFirstByte)
            commitData(SharedBuffer::create());

        if (!frameLoader())
            return;
        frameLoader()->client().finishedLoading(this);
    }

    m_writer.end();
    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();
    if (!frameLoader())
        return;
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

static bool isRedirectToGetAfterPost(const ResourceRequest& oldRequest, const ResourceRequest& newRequest)
{
    return oldRequest.httpMethod() == "POST" && newRequest.httpMethod() == "GET";
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
    Ref<DocumentLoader> protectedThis = Ref { *this };
    
    ResourceResponse response = m_substituteData.response();
    if (response.url().isEmpty())
        response = ResourceResponse(m_request.url(), m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding());

#if ENABLE(CONTENT_EXTENSIONS)
    if (auto* page = m_frame ? m_frame->page() : nullptr) {
        // We intentionally do nothing with the results of this call.
        // We want the CSS to be loaded for us, but we ignore any attempt to block or upgrade the connection since there is no connection.
        page->userContentProvider().processContentRuleListsForLoad(*page, response.url(), ContentExtensions::ResourceType::Document, *this);
    }
#endif

    responseReceived(response, nullptr);
}

#if ENABLE(SERVICE_WORKER)
bool DocumentLoader::setControllingServiceWorkerRegistration(ServiceWorkerRegistrationData&& data)
{
    if (!m_loadingMainResource)
        return false;

    ASSERT(!m_gotFirstByte);
    m_serviceWorkerRegistrationData = WTFMove(data);
    return true;
}

ScriptExecutionContextIdentifier DocumentLoader::resultingClientId() const
{
    return m_resultingClientId;
}

void DocumentLoader::matchRegistration(const URL& url, SWClientConnection::RegistrationCallback&& callback)
{
    auto shouldTryLoadingThroughServiceWorker = !frameLoader()->isReloadingFromOrigin() && m_frame->page() && RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled() && url.protocolIsInHTTPFamily();
    if (!shouldTryLoadingThroughServiceWorker) {
        callback(std::nullopt);
        return;
    }

    auto origin = (!m_frame->isMainFrame() && m_frame->document()) ? m_frame->document()->topOrigin().data() : SecurityOriginData::fromURL(url);
    if (!ServiceWorkerProvider::singleton().serviceWorkerConnection().mayHaveServiceWorkerRegisteredForOrigin(origin)) {
        callback(std::nullopt);
        return;
    }

    auto& connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
    connection.matchRegistration(WTFMove(origin), url, WTFMove(callback));
}

static inline bool areRegistrationsEqual(const std::optional<ServiceWorkerRegistrationData>& a, const std::optional<ServiceWorkerRegistrationData>& b)
{
    if (!a)
        return !b;
    if (!b)
        return false;
    return a->identifier == b->identifier;
}
#endif

void DocumentLoader::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_mainResource);
#if ENABLE(SERVICE_WORKER)
    if (m_serviceWorkerRegistrationData) {
        m_serviceWorkerRegistrationData = { };
        unregisterReservedServiceWorkerClient();
    }

    willSendRequest(WTFMove(request), redirectResponse, [completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }, this] (auto&& request) mutable {
        ASSERT(!m_substituteData.isValid());
        if (request.isNull() || !m_mainDocumentError.isNull() || !m_frame) {
            completionHandler({ });
            return;
        }

        if (m_applicationCacheHost->canLoadMainResource(request)) {
            auto url = request.url();
            // Let's check service worker registration to see whether loading from network or not.
            this->matchRegistration(url, [request = WTFMove(request), completionHandler = WTFMove(completionHandler), protectedThis = WTFMove(protectedThis), this](auto&& registrationData) mutable {
                if (!m_mainDocumentError.isNull() || !m_frame) {
                    completionHandler({ });
                    return;
                }
                if (!registrationData && this->tryLoadingRedirectRequestFromApplicationCache(request)) {
                    completionHandler({ });
                    return;
                }
                completionHandler(WTFMove(request));
            });
            return;
        }
        completionHandler(WTFMove(request));
    });
#else
    willSendRequest(WTFMove(request), redirectResponse, WTFMove(completionHandler));
#endif
}

void DocumentLoader::willSendRequest(ResourceRequest&& newRequest, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());

    // Logging for <rdar://problem/54830233>.
    if (!frameLoader() || !frameLoader()->provisionalDocumentLoader())
        DOCUMENTLOADER_RELEASE_LOG("willSendRequest: With no provisional document loader");

    bool didReceiveRedirectResponse = !redirectResponse.isNull();
    if (!frameLoader()->checkIfFormActionAllowedByCSP(newRequest.url(), didReceiveRedirectResponse, redirectResponse.url())) {
        DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - form action not allowed by CSP");
        cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
        return completionHandler(WTFMove(newRequest));
    }

    ASSERT(timing().startTime());
    if (didReceiveRedirectResponse) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        Ref<SecurityOrigin> redirectingOrigin(SecurityOrigin::create(redirectResponse.url()));
        if (!redirectingOrigin.get().canDisplay(newRequest.url())) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting URL not allowed to display content from target");
            FrameLoader::reportLocalLoadFailed(m_frame.get(), newRequest.url().string());
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
        if (!portAllowed(newRequest.url())) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting to a URL with a blocked port");
            if (m_frame)
                FrameLoader::reportBlockedLoadFailed(*m_frame, newRequest.url());
            cancelMainResourceLoad(frameLoader()->blockedError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
    }

    ASSERT(m_frame);

    Frame& topFrame = m_frame->tree().top();

    ASSERT(m_frame->document());
    ASSERT(topFrame.document());
    
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (m_frame->isMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    FrameLoader::addSameSiteInfoToRequestIfNeeded(newRequest, m_frame->document());

    if (!didReceiveRedirectResponse)
        frameLoader()->client().dispatchWillChangeDocument(m_frame->document()->url(), newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == ResourceRequestCachePolicy::UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);

    if (isRedirectToGetAfterPost(m_request, newRequest))
        newRequest.clearHTTPOrigin();

    if (&topFrame != m_frame) {
        if (!MixedContentChecker::canDisplayInsecureContent(*m_frame, m_frame->document()->securityOrigin(), MixedContentChecker::ContentType::Active, newRequest.url(), MixedContentChecker::AlwaysDisplayInNonStrictMode::Yes)) {
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
        if (!MixedContentChecker::canDisplayInsecureContent(*m_frame, topFrame.document()->securityOrigin(), MixedContentChecker::ContentType::Active, newRequest.url())) {
            cancelMainResourceLoad(frameLoader()->cancelledError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
    }

    if (!newRequest.url().host().isEmpty() && SecurityOrigin::shouldIgnoreHost(newRequest.url())) {
        auto url = newRequest.url();
        url.setHostAndPort({ });
        newRequest.setURL(WTFMove(url));
    }

#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter && !m_contentFilter->continueAfterWillSendRequest(newRequest, redirectResponse))
        return completionHandler(WTFMove(newRequest));
#endif

    setRequest(newRequest);

    if (!didReceiveRedirectResponse)
        return completionHandler(WTFMove(newRequest));

    auto navigationPolicyCompletionHandler = [this, protectedThis = Ref { *this }, protectedFrame = Ref { *m_frame }, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request, WeakPtr<FormState>&&, NavigationPolicyDecision navigationPolicyDecision) mutable {
        m_waitingForNavigationPolicy = false;
        switch (navigationPolicyDecision) {
        case NavigationPolicyDecision::IgnoreLoad:
        case NavigationPolicyDecision::StopAllLoads:
            stopLoadingForPolicyChange();
            break;
        case NavigationPolicyDecision::ContinueLoad:
            break;
        }

        completionHandler(WTFMove(request));
    };

    ASSERT(!m_waitingForNavigationPolicy);
    m_waitingForNavigationPolicy = true;

    // FIXME: Add a load type check.
    auto& policyChecker = frameLoader()->policyChecker();
    RELEASE_ASSERT(!isBackForwardLoadType(policyChecker.loadType()) || frameLoader()->history().provisionalItem());
    policyChecker.checkNavigationPolicy(WTFMove(newRequest), redirectResponse, WTFMove(navigationPolicyCompletionHandler));
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch (Step 12.5.6)
std::optional<CrossOriginOpenerPolicyEnforcementResult> DocumentLoader::doCrossOriginOpenerHandlingOfResponse(const ResourceResponse& response)
{
    // COOP only applies to top-level browsing contexts.
    if (!m_frame->isMainFrame())
        return std::nullopt;

    if (!m_frame->document() || !m_frame->document()->settings().crossOriginOpenerPolicyEnabled())
        return std::nullopt;

    URL openerURL;
    if (auto openerFrame = m_frame->loader().opener())
        openerURL = openerFrame->document() ? openerFrame->document()->url() : URL();

    auto currentCoopEnforcementResult = CrossOriginOpenerPolicyEnforcementResult::from(m_frame->document()->url(), m_frame->document()->securityOrigin(), m_frame->document()->crossOriginOpenerPolicy(), m_triggeringAction.requester(), openerURL);

    auto newCoopEnforcementResult = WebCore::doCrossOriginOpenerHandlingOfResponse(response, m_triggeringAction.requester(), m_contentSecurityPolicy.get(), frameLoader()->effectiveSandboxFlags(), frameLoader()->stateMachine().isDisplayingInitialEmptyDocument(), currentCoopEnforcementResult);
    if (!newCoopEnforcementResult) {
        cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
        return std::nullopt;
    }

    return newCoopEnforcementResult;
}

bool DocumentLoader::tryLoadingRequestFromApplicationCache()
{
    m_applicationCacheHost->maybeLoadMainResource(m_request, m_substituteData);
    return tryLoadingSubstituteData();
}

bool DocumentLoader::tryLoadingSubstituteData()
{
    if (!m_substituteData.isValid() || !m_frame->page())
        return false;

    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Returning substitute data");
    m_identifierForLoadWithoutResourceLoader = ResourceLoaderIdentifier::generate();
    frameLoader()->notifier().assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, this, m_request);
    frameLoader()->notifier().dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, m_request, ResourceResponse(), nullptr);

    if (!m_deferMainResourceDataLoad || frameLoader()->loadsSynchronously())
        handleSubstituteDataLoadNow();
    else {
        auto loadData = [this, weakDataLoadToken = WeakPtr { m_dataLoadToken }] {
            if (!weakDataLoadToken)
                return;
            m_dataLoadToken.clear();
            handleSubstituteDataLoadNow();
        };

#if USE(COCOA_EVENT_LOOP)
        RunLoop::dispatch(*m_frame->page()->scheduledRunLoopPairs(), WTFMove(loadData));
#else
        RunLoop::current().dispatch(WTFMove(loadData));
#endif
    }

    return true;
}

bool DocumentLoader::tryLoadingRedirectRequestFromApplicationCache(const ResourceRequest& request)
{
    m_applicationCacheHost->maybeLoadMainResourceForRedirect(request, m_substituteData);
    if (!m_substituteData.isValid())
        return false;

    RELEASE_ASSERT(m_mainResource);
    auto* loader = m_mainResource->loader();
    m_identifierForLoadWithoutResourceLoader = loader ? loader->identifier() : m_mainResource->identifierForLoadWithoutResourceLoader();

    // We need to remove our reference to the CachedResource in favor of a SubstituteData load, which can triger the cancellation of the underyling ResourceLoader.
    // If the ResourceLoader is indeed cancelled, it would normally send resource load callbacks.
    // Therefore, sever our relationship with the network load but prevent the ResourceLoader from sending ResourceLoadNotifier callbacks.

    RefPtr resourceLoader = mainResourceLoader();
    if (resourceLoader) {
        ASSERT(resourceLoader->shouldSendResourceLoadCallbacks());
        resourceLoader->setSendCallbackPolicy(SendCallbackPolicy::DoNotSendCallbacks);
    }

    clearMainResource();

    if (resourceLoader)
        resourceLoader->setSendCallbackPolicy(SendCallbackPolicy::SendCallbacks);

    handleSubstituteDataLoadNow();
    return true;
}

void DocumentLoader::stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    Ref<DocumentLoader> protectedThis { *this };
    InspectorInstrumentation::continueAfterXFrameOptionsDenied(*m_frame, identifier, *this, response);
    m_frame->document()->enforceSandboxFlags(SandboxOrigin);
    if (HTMLFrameOwnerElement* ownerElement = m_frame->ownerElement())
        ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));

    // The load event might have detached this frame. In that case, the load will already have been cancelled during detach.
    if (FrameLoader* frameLoader = this->frameLoader())
        cancelMainResourceLoad(frameLoader->cancelledError(m_request));
}

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
static URL microsoftTeamsRedirectURL()
{
    return URL(URL(), "https://www.microsoft.com/en-us/microsoft-365/microsoft-teams/");
}
#endif

void DocumentLoader::responseReceived(CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, m_mainResource == &resource);

    if (!response.httpHeaderField(HTTPHeaderName::ContentSecurityPolicy).isNull()) {
        m_contentSecurityPolicy = makeUnique<ContentSecurityPolicy>(URL { response.url() }, nullptr);
        m_contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, m_request.httpReferrer(), ContentSecurityPolicy::ReportParsingErrors::No);
    } else
        m_contentSecurityPolicy = nullptr;
    if (m_frame && m_frame->document() && m_frame->document()->settings().crossOriginOpenerPolicyEnabled())
        m_responseCOOP = obtainCrossOriginOpenerPolicy(response);

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
    // FIXME(218779): Remove this quirk once microsoft.com completes their login flow redesign.
    if (m_frame && m_frame->document()) {
        auto& document = *m_frame->document();
        if (Quirks::isMicrosoftTeamsRedirectURL(response.url())) {
            auto firstPartyDomain = RegistrableDomain(response.url());
            if (auto loginDomains = NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(firstPartyDomain)) {
                if (!Quirks::hasStorageAccessForAllLoginDomains(*loginDomains, firstPartyDomain)) {
                    m_frame->navigationScheduler().scheduleRedirect(document, 0, microsoftTeamsRedirectURL(), IsMetaRefresh::No);
                    return;
                }
            }
        }
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled() && response.source() == ResourceResponse::Source::MemoryCache) {
        matchRegistration(response.url(), [this, protectedThis = Ref { *this }, response, completionHandler = WTFMove(completionHandler)](auto&& registrationData) mutable {
            if (!m_mainDocumentError.isNull() || !m_frame) {
                completionHandler();
                return;
            }
            m_serviceWorkerRegistrationData = WTFMove(registrationData);
            responseReceived(response, WTFMove(completionHandler));
        });
        return;
    }
#endif
    responseReceived(response, WTFMove(completionHandler));
}

void DocumentLoader::responseReceived(const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(response.certificateInfo());
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter && !m_contentFilter->continueAfterResponseReceived(response))
        return;
#endif

    Ref<DocumentLoader> protectedThis(*this);
    bool willLoadFallback = m_applicationCacheHost->maybeLoadFallbackForMainResponse(request(), response);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use.
    if (willLoadFallback)
        MemoryCache::singleton().remove(*m_mainResource);

    if (willLoadFallback)
        return;

    ASSERT(m_identifierForLoadWithoutResourceLoader || m_mainResource);
    ResourceLoaderIdentifier identifier = m_identifierForLoadWithoutResourceLoader ? m_identifierForLoadWithoutResourceLoader : m_mainResource->identifier();
    ASSERT(identifier);

    if (m_substituteData.isValid() || !platformStrategies()->loaderStrategy()->havePerformedSecurityChecks(response)) {
        auto url = response.url();
        ContentSecurityPolicy contentSecurityPolicy(URL { url }, this);
        contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, m_request.httpReferrer());
        if (!contentSecurityPolicy.allowFrameAncestors(*m_frame, url)) {
            stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(identifier, response);
            return;
        }

        if (!contentSecurityPolicy.overridesXFrameOptions()) {
            String frameOptions = response.httpHeaderFields().get(HTTPHeaderName::XFrameOptions);
            if (!frameOptions.isNull()) {
                if (frameLoader()->shouldInterruptLoadForXFrameOptions(frameOptions, url, identifier)) {
                    String message = "Refused to display '" + url.stringCenterEllipsizedToLength() + "' in a frame because it set 'X-Frame-Options' to '" + frameOptions + "'.";
                    m_frame->document()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, identifier.toUInt64());
                    stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(identifier, response);
                    return;
                }
            }
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
        continueAfterContentPolicy(PolicyAction::Use);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    if (m_frame->settings().forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory") {
        continueAfterContentPolicy(PolicyAction::Use);
        return;
    }
#endif

    RefPtr<SubresourceLoader> mainResourceLoader = this->mainResourceLoader();
    if (mainResourceLoader)
        mainResourceLoader->markInAsyncResponsePolicyCheck();
    auto requestIdentifier = PolicyCheckIdentifier::create();
    frameLoader()->checkContentPolicy(m_response, requestIdentifier, [this, protectedThis = Ref { *this }, mainResourceLoader = WTFMove(mainResourceLoader),
        completionHandler = completionHandlerCaller.release(), requestIdentifier] (PolicyAction policy, PolicyCheckIdentifier responseIdentifier) mutable {
        RELEASE_ASSERT(responseIdentifier.isValidFor(requestIdentifier));
        continueAfterContentPolicy(policy);
        if (mainResourceLoader)
            mainResourceLoader->didReceiveResponsePolicy();
        if (completionHandler)
            completionHandler();
    });
}

// Prevent web archives from loading if
// 1) it is remote;
// 2) it is not the main frame;
// 3) it is not any of { loaded by clients; loaded by drag; reloaded from any of the previous two };
// because they can claim to be from any domain and thus avoid cross-domain security checks (4120255, 45524528, 47610130).
bool DocumentLoader::disallowWebArchive() const
{
    using MIMETypeHashSet = HashSet<String, ASCIICaseInsensitiveHash>;
    static NeverDestroyed<MIMETypeHashSet> webArchiveMIMETypes {
        MIMETypeHashSet {
            "application/x-webarchive"_s,
            "application/x-mimearchive"_s,
            "multipart/related"_s,
#if PLATFORM(GTK)
            "message/rfc822"_s,
#endif
        }
    };

    String mimeType = m_response.mimeType();
    if (mimeType.isNull() || !webArchiveMIMETypes.get().contains(mimeType))
        return false;

#if USE(QUICK_LOOK)
    if (isQuickLookPreviewURL(m_response.url()))
        return false;
#endif

    if (m_substituteData.isValid())
        return false;

    if (!LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(m_request.url().protocol().toStringWithoutCopying()))
        return true;

    if (!frame() || (frame()->isMainFrame() && m_allowsWebArchiveForMainFrame))
        return false;

    // On purpose of maintaining existing tests.
    if (frame()->mainFrame().loader().alwaysAllowLocalWebarchive())
        return false;
    return true;
}

// Prevent data URIs from loading as the main frame unless the result of user action.
bool DocumentLoader::disallowDataRequest() const
{
    if (!m_response.url().protocolIsData())
        return false;

    if (!frame() || !frame()->isMainFrame() || m_allowsDataURLsForMainFrame || frame()->settings().allowTopNavigationToDataURLs())
        return false;

    if (auto* currentDocument = frame()->document()) {
        ResourceLoaderIdentifier identifier = m_identifierForLoadWithoutResourceLoader ? m_identifierForLoadWithoutResourceLoader : m_mainResource->identifier();
        ASSERT(identifier);

        currentDocument->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Not allowed to navigate top frame to data URL '", m_response.url().stringCenterEllipsizedToLength(), "'."), identifier.toUInt64());
    }
    DOCUMENTLOADER_RELEASE_LOG("continueAfterContentPolicy: cannot show URL");

    return true;
}

void DocumentLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (isStopping())
        return;

    if (!m_frame) {
        DOCUMENTLOADER_RELEASE_LOG("continueAfterContentPolicy: policyAction=%i received by DocumentLoader with null frame", (int)policy);
        return;
    }

    switch (policy) {
    case PolicyAction::Use: {
        if (!frameLoader()->client().canShowMIMEType(m_response.mimeType()) || disallowWebArchive() || disallowDataRequest()) {
            frameLoader()->policyChecker().cannotShowMIMEType(m_response);
            // Check reachedTerminalState since the load may have already been canceled inside of _handleUnimplementablePolicyWithErrorCode::.
            stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyAction::Download: {
        // m_mainResource can be null, e.g. when loading a substitute resource from application cache.
        if (!m_mainResource) {
            DOCUMENTLOADER_RELEASE_LOG("continueAfterContentPolicy: cannot show URL");
            mainReceivedError(frameLoader()->client().cannotShowURLError(m_request));
            return;
        }

        if (ResourceLoader* mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyDownload(*m_frame, mainResourceLoader->identifier(), *this, m_response);

        // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
        // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
        frameLoader()->setOriginalURLForDownloadRequest(m_request);

        if (m_request.url().protocolIsData()) {
            // We decode data URL internally, there is no resource load to convert.
            frameLoader()->client().startDownload(m_request);
        } else
            frameLoader()->client().convertMainResourceLoadToDownload(this, m_request, m_response);

        // The main resource might be loading from the memory cache, or its loader might have gone missing.
        if (mainResourceLoader()) {
            static_cast<ResourceLoader*>(mainResourceLoader())->didFail(interruptedForPolicyChangeError());
            return;
        }

        // We must stop loading even if there is no main resource loader. Otherwise, we might remain
        // the client of a CachedRawResource that will continue to send us data.
        stopLoadingForPolicyChange();
        return;
    }
    case PolicyAction::StopAllLoads:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        FALLTHROUGH;
#endif
    case PolicyAction::Ignore:
        if (ResourceLoader* mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyIgnore(*m_frame, mainResourceLoader->identifier(), *this, m_response);
        stopLoadingForPolicyChange();
        return;
    }

    if (m_response.isInHTTPFamily()) {
        int status = m_response.httpStatusCode(); // Status may be zero when loading substitute data, in particular from a WebArchive.
        if (status && (status < 200 || status >= 300)) {
            auto* owner = m_frame->ownerElement();
            if (is<HTMLObjectElement>(owner)) {
                downcast<HTMLObjectElement>(*owner).renderFallbackContent();
                // object elements are no longer rendered after we fallback, so don't
                // keep trying to process data from their load
                cancelMainResourceLoad(frameLoader()->cancelledError(m_request));
            }
        }
    }

    if (!isStopping() && m_substituteData.isValid() && isLoadingMainResource()) {
        auto content = m_substituteData.content();
        if (content && content->size()) {
            content->forEachSegmentAsSharedBuffer([&](auto&& buffer) {
                dataReceived(buffer);
            });
        }
        if (isLoadingMainResource())
            finishedLoading();

        // Remove ourselves as a client of this CachedResource as we've decided to commit substitute data but the
        // load may keep going and be useful to other clients of the CachedResource. If we did not do this, we
        // may receive data later on even though this DocumentLoader has finished loading.
        clearMainResource();
    }
}

void DocumentLoader::commitLoad(const SharedBuffer& data)
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    RefPtr<Frame> protectedFrame(m_frame.get());
    Ref<DocumentLoader> protectedThis(*this);

    commitIfReady();
    FrameLoader* frameLoader = DocumentLoader::frameLoader();
    if (!frameLoader)
        return;
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (ArchiveFactory::isArchiveMIMEType(response().mimeType()))
        return;
#endif
    frameLoader->client().committedLoad(this, data);

    if (isMultipartReplacingLoad())
        frameLoader->client().didReplaceMultipartContent();
}

ResourceError DocumentLoader::interruptedForPolicyChangeError() const
{
    if (!frameLoader())
        return {};

    return frameLoader()->client().interruptedForPolicyChangeError(request());
}

void DocumentLoader::stopLoadingForPolicyChange()
{
    ResourceError error = interruptedForPolicyChangeError();
    error.setType(ResourceError::Type::Cancellation);
    cancelMainResourceLoad(error);
}

#if ENABLE(SERVICE_WORKER)
// https://w3c.github.io/ServiceWorker/#control-and-use-window-client
static inline bool shouldUseActiveServiceWorkerFromParent(const Document& document, const Document& parent)
{
    return !document.url().protocolIsInHTTPFamily() && !document.securityOrigin().isUnique() && parent.securityOrigin().isSameOriginDomain(document.securityOrigin());
}
#endif

void DocumentLoader::commitData(const SharedBuffer& data)
{
    if (!m_gotFirstByte) {
        m_gotFirstByte = true;
        bool hasBegun = m_writer.begin(documentURL(), false, nullptr, m_resultingClientId);
        if (!hasBegun)
            return;

        m_writer.setDocumentWasLoadedAsPartOfNavigation();

        auto* documentOrNull = m_frame ? m_frame->document() : nullptr;
        if (!documentOrNull)
            return;
        auto& document = *documentOrNull;

        if (SecurityPolicy::allowSubstituteDataAccessToLocal() && m_originalSubstituteDataWasValid) {
            // If this document was loaded with substituteData, then the document can
            // load local resources. See https://bugs.webkit.org/show_bug.cgi?id=16756
            // and https://bugs.webkit.org/show_bug.cgi?id=19760 for further
            // discussion.
            document.securityOrigin().grantLoadLocalResources();
        }

        if (frameLoader()->stateMachine().creatingInitialEmptyDocument())
            return;

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
        if (m_archive && m_archive->shouldOverrideBaseURL())
            document.setBaseURLOverride(m_archive->mainResource()->url());
#endif
#if ENABLE(SERVICE_WORKER)
        if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled()) {
            if (!document.securityOrigin().isUnique()) {
                if (m_serviceWorkerRegistrationData && m_serviceWorkerRegistrationData->activeWorker) {
                    document.setActiveServiceWorker(ServiceWorker::getOrCreate(document, WTFMove(m_serviceWorkerRegistrationData->activeWorker.value())));
                    m_serviceWorkerRegistrationData = { };
                } else if (auto* parent = document.parentDocument()) {
                    if (shouldUseActiveServiceWorkerFromParent(document, *parent))
                        document.setActiveServiceWorker(parent->activeServiceWorker());
                }
            } else if (m_resultingClientId) {
                // In case document has a unique origin, say due to sandboxing, we should have created a new context, let's create a new identifier instead.
                if (document.securityOrigin().isUnique())
                    document.createNewIdentifier();
            }

            if (m_frame->document()->activeServiceWorker() || document.url().protocolIsInHTTPFamily() || (document.page() && document.page()->isServiceWorkerPage()))
                document.setServiceWorkerConnection(&ServiceWorkerProvider::singleton().serviceWorkerConnection());

            if (m_resultingClientId) {
                if (m_resultingClientId != document.identifier())
                    unregisterReservedServiceWorkerClient();
                scriptExecutionContextIdentifierToLoaderMap().remove(m_resultingClientId);
                m_resultingClientId = { };
            }
        }
#endif
        // Call receivedFirstData() exactly once per load. We should only reach this point multiple times
        // for multipart loads, and FrameLoader::isReplacing() will be true after the first time.
        if (!isMultipartReplacingLoad())
            frameLoader()->receivedFirstData();

        // The load could be canceled under receivedFirstData(), which makes delegate calls and even sometimes dispatches DOM events.
        if (!isLoading())
            return;

        if (auto* window = document.domWindow()) {
            window->prewarmLocalStorageIfNecessary();

            if (document.settings().performanceNavigationTimingAPIEnabled() && m_mainResource) {
                auto* metrics = m_response.deprecatedNetworkLoadMetricsOrNull();
                window->performance().addNavigationTiming(*this, document, *m_mainResource, timing(), metrics ? *metrics : NetworkLoadMetrics::emptyMetrics());
            }
        }

        DocumentWriter::IsEncodingUserChosen userChosen;
        String encoding;
        if (overrideEncoding().isNull()) {
            userChosen = DocumentWriter::IsEncodingUserChosen::No;
            encoding = response().textEncodingName();
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
            if (m_archive && m_archive->shouldUseMainResourceEncoding())
                encoding = m_archive->mainResource()->textEncoding();
#endif
        } else {
            userChosen = DocumentWriter::IsEncodingUserChosen::Yes;
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
    m_writer.addData(data);
}

void DocumentLoader::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    ASSERT_UNUSED(resource, &resource == m_mainResource);
    dataReceived(buffer);
}

void DocumentLoader::dataReceived(const SharedBuffer& buffer)
{
#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter && !m_contentFilter->continueAfterDataReceived(buffer))
        return;
#endif

    ASSERT(buffer.data());
    ASSERT(buffer.size());
    ASSERT(!m_response.isNull());

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    if (m_identifierForLoadWithoutResourceLoader)
        frameLoader()->notifier().dispatchDidReceiveData(this, m_identifierForLoadWithoutResourceLoader, &buffer, buffer.size(), -1);

    m_applicationCacheHost->mainResourceDataReceived(buffer, -1, false);

    if (!isMultipartReplacingLoad())
        commitLoad(buffer);
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

#if ENABLE(SERVICE_WORKER)
    unregisterReservedServiceWorkerClient();
    if (m_resultingClientId) {
        scriptExecutionContextIdentifierToLoaderMap().remove(m_resultingClientId);
        m_resultingClientId = { };
    }
#endif

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

void DocumentLoader::applyPoliciesToSettings()
{
    if (!m_frame) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (!m_frame->isMainFrame())
        return;

#if ENABLE(MEDIA_SOURCE)
    m_frame->settings().setMediaSourceEnabled(m_mediaSourcePolicy == MediaSourcePolicy::Default ? Settings::platformDefaultMediaSourceEnabled() : m_mediaSourcePolicy == MediaSourcePolicy::Enable);
#endif
#if ENABLE(TEXT_AUTOSIZING)
    m_frame->settings().setIdempotentModeAutosizingOnlyHonorsPercentages(m_idempotentModeAutosizingOnlyHonorsPercentages);
#endif
#if USE(APPLE_INTERNAL_SDK)
    updateAdditionalSettingsIfNeeded();
#endif
}

MouseEventPolicy DocumentLoader::mouseEventPolicy() const
{
#if ENABLE(IOS_TOUCH_EVENTS)
    if (m_mouseEventPolicy == MouseEventPolicy::Default) {
        if (auto* document = this->document()) {
            if (document->quirks().shouldSynthesizeTouchEvents())
                return MouseEventPolicy::SynthesizeTouchEvents;
        }
    }
#endif
    return m_mouseEventPolicy;
}

ColorSchemePreference DocumentLoader::colorSchemePreference() const
{
    return m_colorSchemePreference;
}

void DocumentLoader::attachToFrame(Frame& frame)
{
    if (m_frame == &frame)
        return;

    ASSERT(!m_frame);
    observeFrame(&frame);
    m_writer.setFrame(frame);
    attachToFrame();

#if ASSERT_ENABLED
    m_hasEverBeenAttached = true;
#endif

    applyPoliciesToSettings();
}

void DocumentLoader::attachToFrame()
{
    ASSERT(m_frame);
    DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::attachToFrame: m_frame=%p", m_frame.get());
}

void DocumentLoader::detachFromFrame()
{
    DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::detachFromFrame: m_frame=%p", m_frame.get());

#if ASSERT_ENABLED
    if (m_hasEverBeenAttached)
        ASSERT_WITH_MESSAGE(m_frame, "detachFromFrame() is being called on a DocumentLoader twice without an attachToFrame() inbetween");
    else
        ASSERT_WITH_MESSAGE(m_frame, "detachFromFrame() is being called on a DocumentLoader that has never attached to any Frame");
#endif
    RefPtr<Frame> protectedFrame(m_frame.get());
    Ref<DocumentLoader> protectedThis(*this);

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so kill all the loads.
    stopLoading();
    if (m_mainResource && m_mainResource->hasClient(*this))
        m_mainResource->removeClient(*this);
#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    m_applicationCacheHost->setDOMApplicationCache(nullptr);

    cancelPolicyCheckIfNeeded();

    // cancelPolicyCheckIfNeeded can clear m_frame if the policy check
    // is stopped, resulting in a recursive call into this detachFromFrame.
    // If m_frame is nullptr after cancelPolicyCheckIfNeeded, our work is
    // already done so just return.
    if (!m_frame)
        return;

    InspectorInstrumentation::loaderDetachedFromFrame(*m_frame, *this);

    observeFrame(nullptr);
}

void DocumentLoader::clearMainResourceLoader()
{
    m_loadingMainResource = false;

    auto* frameLoader = this->frameLoader();

    if (!frameLoader)
        return;

    if (this == frameLoader->activeDocumentLoader())
        checkLoadComplete();
}

#if ENABLE(APPLICATION_MANIFEST)

void DocumentLoader::loadApplicationManifest(CompletionHandler<void(const std::optional<ApplicationManifest>&)>&& completionHandler)
{
    if (completionHandler)
        m_loadApplicationManifestCallbacks.append(WTFMove(completionHandler));

    bool isLoading = !!m_applicationManifestLoader;
    auto notifyIfUnableToLoad = makeScopeExit([&] {
        if (!isLoading || m_finishedLoadingApplicationManifest)
            notifyFinishedLoadingApplicationManifest();
    });

    if (isLoading)
        return;

    auto* document = this->document();
    if (!document)
        return;

    if (!document->isTopDocument())
        return;

    if (document->url().isEmpty() || document->url().protocolIsAbout())
        return;

    auto head = document->head();
    if (!head)
        return;

    URL manifestURL;
    bool useCredentials = false;
    for (const auto& link : childrenOfType<HTMLLinkElement>(*head)) {
        if (!link.isApplicationManifest())
            continue;

        auto href = link.href();
        if (href.isEmpty() || !href.isValid())
            continue;

        if (!link.mediaAttributeMatches())
            continue;

        manifestURL = href;
        useCredentials = equalIgnoringASCIICase(link.attributeWithoutSynchronization(HTMLNames::crossoriginAttr), "use-credentials");
        break;
    }

    if (manifestURL.isEmpty() || !manifestURL.isValid())
        return;

    m_applicationManifestLoader = makeUnique<ApplicationManifestLoader>(*this, manifestURL, useCredentials);

    isLoading = m_applicationManifestLoader->startLoading();
    if (!isLoading)
        m_finishedLoadingApplicationManifest = true;
}

void DocumentLoader::finishedLoadingApplicationManifest(ApplicationManifestLoader& loader)
{
    ASSERT_UNUSED(loader, &loader == m_applicationManifestLoader.get());

    // If the DocumentLoader has detached from its frame, all manifest loads should have already been canceled.
    ASSERT(m_frame);

    ASSERT(!m_finishedLoadingApplicationManifest);
    m_finishedLoadingApplicationManifest = true;

    notifyFinishedLoadingApplicationManifest();
}

void DocumentLoader::notifyFinishedLoadingApplicationManifest()
{
    std::optional<ApplicationManifest> manifest = m_applicationManifestLoader ? m_applicationManifestLoader->processManifest() : std::nullopt;
    ASSERT_IMPLIES(manifest, m_finishedLoadingApplicationManifest);

    for (auto& callback : std::exchange(m_loadApplicationManifestCallbacks, { }))
        callback(manifest);
}

#endif // ENABLE(APPLICATION_MANIFEST)

bool DocumentLoader::isLoadingInAPISense() const
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if (frameLoader()->state() != FrameState::Complete) {
        ASSERT(m_frame->document());
        auto& document = *m_frame->document();
        if ((isLoadingMainResource() || !document.loadEventFinished()) && isLoading())
            return true;
        if (m_cachedResourceLoader->requestCount())
            return true;
        if (document.isDelayingLoadEvent())
            return true;
        if (document.processingLoadEvent())
            return true;
        if (document.hasActiveParser())
            return true;
        auto* scriptableParser = document.scriptableDocumentParser();
        if (scriptableParser && scriptableParser->hasScriptsWaitingForStylesheets())
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
    
    addAllArchiveResources(*m_archive);
    ASSERT(m_archive->mainResource());
    auto& mainResource = *m_archive->mainResource();
    m_parsedArchiveData = mainResource.data().makeContiguous();
    m_writer.setMIMEType(mainResource.mimeType());

    ASSERT(m_frame->document());
    commitData(*m_parsedArchiveData);
    return true;
#endif
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

void DocumentLoader::setArchive(Ref<Archive>&& archive)
{
    m_archive = WTFMove(archive);
    addAllArchiveResources(*m_archive);
}

void DocumentLoader::addAllArchiveResources(Archive& archive)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = makeUnique<ArchiveResourceCollection>();
    m_archiveResourceCollection->addAllResources(archive);
}

// FIXME: Adding a resource directly to a DocumentLoader/ArchiveResourceCollection seems like bad design, but is API some apps rely on.
// Can we change the design in a manner that will let us deprecate that API without reducing functionality of those apps?
void DocumentLoader::addArchiveResource(Ref<ArchiveResource>&& resource)
{
    if (!m_archiveResourceCollection)
        m_archiveResourceCollection = makeUnique<ArchiveResourceCollection>();
    m_archiveResourceCollection->addResource(WTFMove(resource));
}

RefPtr<Archive> DocumentLoader::popArchiveForSubframe(const String& frameName, const URL& url)
{
    return m_archiveResourceCollection ? m_archiveResourceCollection->popSubframeArchive(frameName, url) : nullptr;
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
    auto* resource = m_archiveResourceCollection->archiveResourceForURL(url);
    if (!resource || resource->shouldIgnoreWhenUnarchiving())
        return nullptr;
    return resource;
}

RefPtr<ArchiveResource> DocumentLoader::mainResource() const
{
    RefPtr<FragmentedSharedBuffer> data = mainResourceData();
    if (!data)
        data = SharedBuffer::create();
    auto& response = this->response();
    return ArchiveResource::create(WTFMove(data), response.url(), response.mimeType(), response.textEncodingName(), frame()->tree().uniqueName());
}

RefPtr<ArchiveResource> DocumentLoader::subresource(const URL& url) const
{
    if (!isCommitted())
        return nullptr;
    
    auto* resource = m_cachedResourceLoader->cachedResource(url);
    if (!resource || !resource->isLoaded())
        return archiveResourceForURL(url);

    if (resource->type() == CachedResource::Type::MainResource)
        return nullptr;

    auto* data = resource->resourceBuffer();
    if (!data)
        return nullptr;

    return ArchiveResource::create(data, url, resource->response());
}

Vector<Ref<ArchiveResource>> DocumentLoader::subresources() const
{
    if (!isCommitted())
        return { };

    Vector<Ref<ArchiveResource>> subresources;
    for (auto& handle : m_cachedResourceLoader->allCachedResources().values()) {
        if (auto subresource = this->subresource(handle->url()))
            subresources.append(subresource.releaseNonNull());
    }
    return subresources;
}

void DocumentLoader::deliverSubstituteResourcesAfterDelay()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame);
    ASSERT(m_frame->page());
    if (m_frame->page()->defersLoading())
        return;

    if (!m_substituteResourceDeliveryTimer.isActive())
        m_substituteResourceDeliveryTimer.startOneShot(0_s);
}

void DocumentLoader::substituteResourceDeliveryTimerFired()
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    ASSERT(m_frame);
    ASSERT(m_frame->page());
    if (m_frame->page()->defersLoading())
        return;

    auto pendingSubstituteResources = WTFMove(m_pendingSubstituteResources);
    for (auto& pendingSubstituteResource : pendingSubstituteResources) {
        auto& loader = pendingSubstituteResource.key;
        if (auto& resource = pendingSubstituteResource.value)
            resource->deliver(*loader);
        else {
            // A null resource means that we should fail the load.
            // FIXME: Maybe we should use another error here - something like "not in cache".
            loader->didFail(loader->cannotShowURLError());
        }
    }
}

#if ASSERT_ENABLED

bool DocumentLoader::isSubstituteLoadPending(ResourceLoader* loader) const
{
    return m_pendingSubstituteResources.contains(loader);
}

#endif // ASSERT_ENABLED

void DocumentLoader::cancelPendingSubstituteLoad(ResourceLoader* loader)
{
    if (m_pendingSubstituteResources.isEmpty())
        return;
    m_pendingSubstituteResources.remove(loader);
    if (m_pendingSubstituteResources.isEmpty())
        m_substituteResourceDeliveryTimer.stop();
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

bool DocumentLoader::scheduleArchiveLoad(ResourceLoader& loader, const ResourceRequest& request)
{
    if (auto* resource = archiveResourceForURL(request.url())) {
        scheduleSubstituteResourceLoad(loader, *resource);
        return true;
    }

    if (!m_archive)
        return false;

#if ENABLE(WEB_ARCHIVE)
    // The idea of WebArchiveDebugMode is that we should fail instead of trying to fetch from the network.
    // Returning true ensures the caller will not try to fetch from the network.
    if (m_frame->settings().webArchiveDebugModeEnabled() && responseMIMEType() == "application/x-webarchive")
        return true;
#endif

    // If we want to load from the archive only, then we should always return true so that the caller
    // does not try to fetch form the network.
    return m_archive->shouldLoadFromArchiveOnly();
}

#endif

void DocumentLoader::scheduleSubstituteResourceLoad(ResourceLoader& loader, SubstituteResource& resource)
{
#if ENABLE(SERVICE_WORKER)
    ASSERT(!loader.options().serviceWorkerRegistrationIdentifier);
#endif
    m_pendingSubstituteResources.set(&loader, &resource);
    deliverSubstituteResourcesAfterDelay();
}

void DocumentLoader::scheduleCannotShowURLError(ResourceLoader& loader)
{
    m_pendingSubstituteResources.set(&loader, nullptr);
    deliverSubstituteResourcesAfterDelay();
}

void DocumentLoader::addResponse(const ResourceResponse& response)
{
    if (!m_stopRecordingResponses)
        m_responses.append(response);
}

void DocumentLoader::stopRecordingResponses()
{
    m_stopRecordingResponses = true;
    m_responses.shrinkToFit();
}

void DocumentLoader::setCustomHeaderFields(Vector<CustomHeaderFields>&& fields)
{
    m_customHeaderFields = WTFMove(fields);
}

void DocumentLoader::setTitle(const StringWithDirection& title)
{
    if (m_pageTitle == title)
        return;

    frameLoader()->willChangeTitle(this);
    m_pageTitle = title;
    if (frameLoader())
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

URL DocumentLoader::documentURL() const
{
    URL url = substituteData().response().url();
#if ENABLE(WEB_ARCHIVE)
    if (url.isEmpty() && m_archive && m_archive->shouldUseMainResourceURL())
        url = m_archive->mainResource()->url();
#endif
    if (url.isEmpty())
        url = m_request.url();
    if (url.isEmpty())
        url = m_response.url();
    return url;
}

#if PLATFORM(IOS_FAMILY)

// FIXME: This method seems to violate the encapsulation of this class.
void DocumentLoader::setResponseMIMEType(const String& responseMimeType)
{
    m_response.setMimeType(responseMimeType);
}

#endif

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

void DocumentLoader::addSubresourceLoader(ResourceLoader& loader)
{
    // The main resource's underlying ResourceLoader will ask to be added here.
    // It is much simpler to handle special casing of main resource loads if we don't
    // let it be added. In the main resource load case, mainResourceLoader()
    // will still be null at this point, but m_gotFirstByte should be false here if and only
    // if we are just starting the main resource load.
    if (!m_gotFirstByte)
        return;
    ASSERT(loader.identifier());
    ASSERT(!m_subresourceLoaders.contains(loader.identifier()));
    ASSERT(!mainResourceLoader() || mainResourceLoader() != &loader);

    // Application Cache loaders are handled by their ApplicationCacheGroup directly.
    if (loader.options().applicationCacheMode == ApplicationCacheMode::Bypass)
        return;

#if ASSERT_ENABLED
    if (document()) {
        switch (document()->backForwardCacheState()) {
        case Document::NotInBackForwardCache:
            break;
        case Document::AboutToEnterBackForwardCache: {
            // A page about to enter the BackForwardCache should only be able to start ping loads.
            auto* cachedResource = downcast<SubresourceLoader>(loader).cachedResource();
            ASSERT(cachedResource && (CachedResource::shouldUsePingLoad(cachedResource->type()) || cachedResource->options().keepAlive));
            break;
        }
        case Document::InBackForwardCache:
            // A page in the BackForwardCache should not be able to start loads.
            ASSERT_NOT_REACHED();
            break;
        }
    }
#endif

    m_subresourceLoaders.add(loader.identifier(), &loader);
}

void DocumentLoader::removeSubresourceLoader(LoadCompletionType type, ResourceLoader* loader)
{
    ASSERT(loader->identifier());

    if (!m_subresourceLoaders.remove(loader->identifier()))
        return;
    checkLoadComplete();
    if (m_frame)
        m_frame->loader().subresourceLoadDone(type);
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
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || LegacySchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol().toStringWithoutCopying()));
    if (!shouldLoadEmpty && !frameLoader()->client().representationExistsForURLScheme(m_request.url().protocol().toStringWithoutCopying()))
        return false;

    if (m_request.url().isEmpty() && !frameLoader()->stateMachine().creatingInitialEmptyDocument()) {
        m_request.setURL(aboutBlankURL());
        if (isLoadingMainResource())
            frameLoader()->client().dispatchDidChangeProvisionalURL();
    }

    String mimeType = shouldLoadEmpty ? "text/html" : frameLoader()->client().generatedMIMETypeForURLScheme(m_request.url().protocol().toStringWithoutCopying());
    m_response = ResourceResponse(m_request.url(), mimeType, 0, "UTF-8"_s);

    if (!frameLoader()->stateMachine().isDisplayingInitialEmptyDocument()) {
        if (auto coopEnforcementResult = doCrossOriginOpenerHandlingOfResponse(m_response)) {
            m_responseCOOP = coopEnforcementResult->crossOriginOpenerPolicy;
            if (coopEnforcementResult->needsBrowsingContextGroupSwitch)
                frameLoader()->switchBrowsingContextsGroup();
        }
    }

    finishedLoading();
    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    m_mainDocumentError = ResourceError();
    timing().markStartTime();
    ASSERT(!m_mainResource);
    ASSERT(!m_loadingMainResource);
    m_loadingMainResource = true;

    Ref<DocumentLoader> protectedThis(*this);

    if (maybeLoadEmpty()) {
        DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Returning empty document");
        return;
    }

#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    m_contentFilter = !m_substituteData.isValid() ? ContentFilter::create(*this) : nullptr;
#endif

    // Make sure we re-apply the user agent to the Document's ResourceRequest upon reload in case the embedding
    // application has changed it, by clearing the previous user agent value here and applying the new value in CachedResourceLoader.
    m_request.clearHTTPUserAgent();

    ASSERT(timing().startTime());

    willSendRequest(ResourceRequest(m_request), ResourceResponse(), [this, protectedThis = WTFMove(protectedThis)] (ResourceRequest&& request) mutable {
        m_request = request;
        // FIXME: Implement local URL interception by getting the service worker of the parent.

        // willSendRequest() may lead to our Frame being detached or cancelling the load via nulling the ResourceRequest.
        if (!m_frame || m_request.isNull()) {
            DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Load canceled after willSendRequest");
            return;
        }

        request.setRequester(ResourceRequest::Requester::Main);
        // If this is a reload the cache layer might have made the previous request conditional. DocumentLoader can't handle 304 responses itself.
        request.makeUnconditional();

        DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Starting load");

#if ENABLE(SERVICE_WORKER)
        if (m_applicationCacheHost->canLoadMainResource(request) || m_substituteData.isValid()) {
            auto url = request.url();
            matchRegistration(url, [request = WTFMove(request), protectedThis = WTFMove(protectedThis), this] (auto&& registrationData) mutable {
                if (!m_mainDocumentError.isNull()) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because of main document error (type=%d, code=%d)", static_cast<int>(m_mainDocumentError.type()), m_mainDocumentError.errorCode());
                    return;
                }
                if (!m_frame) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because no frame");
                    return;
                }

                m_serviceWorkerRegistrationData = WTFMove(registrationData);
                // Prefer existing substitute data (from WKWebView.loadData etc) over service worker fetch.
                if (this->tryLoadingSubstituteData()) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because of substitute data");
                    return;
                }

                if (!m_serviceWorkerRegistrationData && this->tryLoadingRequestFromApplicationCache()) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Loaded from Application Cache");
                    return;
                }
                this->loadMainResource(WTFMove(request));
            });
            return;
        }
#else
        if (tryLoadingRequestFromApplicationCache()) {
            DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Loaded from Application Cache");
            return;
        }
#endif
        loadMainResource(WTFMove(request));
    });
}

void DocumentLoader::unregisterReservedServiceWorkerClient()
{
#if ENABLE(SERVICE_WORKER)
    if (!m_resultingClientId || !RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled())
        return;

    auto& serviceWorkerConnection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
    serviceWorkerConnection.unregisterServiceWorkerClient(m_resultingClientId);
#endif
}

void DocumentLoader::loadMainResource(ResourceRequest&& request)
{
    ResourceLoaderOptions mainResourceLoadOptions(
        SendCallbackPolicy::SendCallbacks,
        ContentSniffingPolicy::SniffContent,
        DataBufferingPolicy::BufferData,
        StoredCredentialsPolicy::Use,
        ClientCredentialPolicy::MayAskClientForCredentials,
        FetchOptions::Credentials::Include,
        SecurityCheckPolicy::SkipSecurityCheck,
        FetchOptions::Mode::Navigate,
        CertificateInfoPolicy::IncludeCertificateInfo,
        ContentSecurityPolicyImposition::SkipPolicyCheck,
        DefersLoadingPolicy::AllowDefersLoading,
        CachingPolicy::AllowCaching);

#if ENABLE(SERVICE_WORKER)
    if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled()) {
        // The main navigation load will trigger the registration of the client.
        if (m_resultingClientId)
            scriptExecutionContextIdentifierToLoaderMap().remove(m_resultingClientId);
        m_resultingClientId = ScriptExecutionContextIdentifier::generate();
        ASSERT(!scriptExecutionContextIdentifierToLoaderMap().contains(m_resultingClientId));
        scriptExecutionContextIdentifierToLoaderMap().add(m_resultingClientId, this);
    }
#endif

    CachedResourceRequest mainResourceRequest(WTFMove(request), mainResourceLoadOptions);
    if (!m_frame->isMainFrame() && m_frame->document()) {
        // If we are loading the main resource of a subframe, use the cache partition of the main document.
        mainResourceRequest.setDomainForCachePartition(*m_frame->document());
    } else {
        auto origin = SecurityOrigin::create(mainResourceRequest.resourceRequest().url());
        origin->setStorageBlockingPolicy(frameLoader()->frame().settings().storageBlockingPolicy());
        mainResourceRequest.setDomainForCachePartition(origin->domainForCachePartition());
    }

    m_mainResource = m_cachedResourceLoader->requestMainResource(WTFMove(mainResourceRequest)).value_or(nullptr);

    if (!m_mainResource) {
        // The frame may have gone away if this load was cancelled synchronously and this was the last pending load.
        // This is because we may have fired the load event in a parent frame.
        if (!m_frame) {
            DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, frame has gone away");
            return;
        }

        if (!m_request.url().isValid()) {
            DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, URL is invalid");
            cancelMainResourceLoad(frameLoader()->client().cannotShowURLError(m_request));
            return;
        }

        DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, returning empty document");

        setRequest(ResourceRequest());
        // If the load was aborted by clearing m_request, it's possible the ApplicationCacheHost
        // is now in a state where starting an empty load will be inconsistent. Replace it with
        // a new ApplicationCacheHost.
        m_applicationCacheHost = makeUnique<ApplicationCacheHost>(*this);
        maybeLoadEmpty();
        return;
    }

    ASSERT(m_frame);

#if ENABLE(CONTENT_EXTENSIONS)
    if (m_mainResource->errorOccurred() && m_frame->page() && m_mainResource->resourceError().domain() == ContentExtensions::WebKitContentBlockerDomain) {
        DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Blocked by content blocker error");
        cancelMainResourceLoad(frameLoader()->blockedByContentBlockerError(m_request));
        return;
    }
#endif

    if (!mainResourceLoader()) {
        m_identifierForLoadWithoutResourceLoader = ResourceLoaderIdentifier::generate();
        frameLoader()->notifier().assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, this, mainResourceRequest.resourceRequest());
        frameLoader()->notifier().dispatchWillSendRequest(this, m_identifierForLoadWithoutResourceLoader, mainResourceRequest.resourceRequest(), ResourceResponse(), nullptr);
    }

    becomeMainResourceClient();

    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    ResourceRequest updatedRequest = mainResourceLoader() ? mainResourceLoader()->originalRequest() : mainResourceRequest.resourceRequest();
    // If there was a fragment identifier on m_request, the cache will have stripped it. m_request should include
    // the fragment identifier, so add that back in.
    if (equalIgnoringFragmentIdentifier(m_request.url(), updatedRequest.url()))
        updatedRequest.setURL(m_request.url());
    setRequest(updatedRequest);
}

void DocumentLoader::cancelPolicyCheckIfNeeded()
{
    if (m_waitingForContentPolicy || m_waitingForNavigationPolicy) {
        RELEASE_ASSERT(frameLoader());
        frameLoader()->policyChecker().stopCheck();
        m_waitingForContentPolicy = false;
        m_waitingForNavigationPolicy = false;
    }
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& resourceError)
{
    Ref<DocumentLoader> protectedThis(*this);
    ResourceError error = resourceError.isNull() ? frameLoader()->cancelledError(m_request) : resourceError;

    DOCUMENTLOADER_RELEASE_LOG("cancelMainResourceLoad: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

    m_dataLoadToken.clear();

    cancelPolicyCheckIfNeeded();

    if (mainResourceLoader())
        mainResourceLoader()->cancel(error);

    clearMainResource();

    mainReceivedError(error);
}

void DocumentLoader::willContinueMainResourceLoadAfterRedirect(const ResourceRequest& newRequest)
{
    setRequest(newRequest);
}

void DocumentLoader::clearMainResource()
{
    ASSERT(isMainThread());
    if (m_mainResource && m_mainResource->hasClient(*this))
        m_mainResource->removeClient(*this);
#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    m_mainResource = nullptr;
    m_isContinuingLoadAfterProvisionalLoadStarted = false;

    unregisterReservedServiceWorkerClient();
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader& loader)
{
    ResourceLoaderIdentifier identifier = loader.identifier();
    ASSERT(identifier);

    if (!m_multipartSubresourceLoaders.add(identifier, &loader).isNewEntry) {
        ASSERT(m_multipartSubresourceLoaders.get(identifier) == &loader);
        ASSERT(!m_subresourceLoaders.contains(identifier));
    } else {
        ASSERT(m_subresourceLoaders.contains(identifier));
        m_subresourceLoaders.remove(identifier);
    }

    checkLoadComplete();
    if (m_frame)
        m_frame->loader().checkLoadComplete();
}

void DocumentLoader::maybeFinishLoadingMultipartContent()
{
    if (!isMultipartReplacingLoad())
        return;

    frameLoader()->setupForReplace();
    m_committed = false;
    commitLoad(mainResourceData()->makeContiguous());
}

void DocumentLoader::startIconLoading()
{
    static uint64_t nextIconCallbackID = 1;

    auto* document = this->document();
    if (!document)
        return;

    if (!m_frame->isMainFrame())
        return;

    if (document->url().isEmpty() || document->url().protocolIsAbout())
        return;

    m_linkIcons = LinkIconCollector { *document }.iconsOfTypes({ LinkIconType::Favicon, LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon });

    auto findResult = m_linkIcons.findIf([](auto& icon) { return icon.type == LinkIconType::Favicon; });
    if (findResult == notFound)
        m_linkIcons.append({ document->completeURL("/favicon.ico"_s), LinkIconType::Favicon, String(), std::nullopt, { } });

    if (!m_linkIcons.size())
        return;

    auto iconDecisions = WTF::map(m_linkIcons, [&](auto& icon) -> std::pair<WebCore::LinkIcon&, uint64_t> {
        auto result = m_iconsPendingLoadDecision.add(nextIconCallbackID++, icon);
        return { icon, result.iterator->key };
    });
    m_frame->loader().client().getLoadDecisionForIcons(WTFMove(iconDecisions));
}

void DocumentLoader::didGetLoadDecisionForIcon(bool decision, uint64_t loadIdentifier, CompletionHandler<void(FragmentedSharedBuffer*)>&& completionHandler)
{
    auto icon = m_iconsPendingLoadDecision.take(loadIdentifier);

    // If the decision was not to load or this DocumentLoader is already detached, there is no load to perform.
    if (!decision || !m_frame)
        return completionHandler(nullptr);

    // If the LinkIcon we just took is empty, then the DocumentLoader had all of its loaders stopped
    // while this icon load decision was pending.
    // In this case we need to notify the client that the icon finished loading with empty data.
    if (icon.url.isEmpty())
        return completionHandler(nullptr);

    auto iconLoader = makeUnique<IconLoader>(*this, icon.url);
    auto* rawIconLoader = iconLoader.get();
    m_iconLoaders.add(WTFMove(iconLoader), WTFMove(completionHandler));

    rawIconLoader->startLoading();
}

void DocumentLoader::finishedLoadingIcon(IconLoader& loader, FragmentedSharedBuffer* buffer)
{
    // If the DocumentLoader has detached from its frame, all icon loads should have already been cancelled.
    ASSERT(m_frame);

    if (auto callback = m_iconLoaders.take(&loader))
        callback(buffer);
}

void DocumentLoader::dispatchOnloadEvents()
{
    m_wasOnloadDispatched = true;
    m_applicationCacheHost->stopDeferringEvents();
}

void DocumentLoader::setTriggeringAction(NavigationAction&& action)
{
    m_triggeringAction = WTFMove(action);
    m_triggeringAction.setShouldOpenExternalURLsPolicy(m_frame ? shouldOpenExternalURLsPolicyToPropagate() : m_shouldOpenExternalURLsPolicy);
}

ShouldOpenExternalURLsPolicy DocumentLoader::shouldOpenExternalURLsPolicyToPropagate() const
{
    if (!m_frame)
        return ShouldOpenExternalURLsPolicy::ShouldNotAllow;

    if (m_frame->isMainFrame())
        return m_shouldOpenExternalURLsPolicy;

    if (document() && document()->isSameOriginAsTopDocument())
        return m_shouldOpenExternalURLsPolicy;

    return ShouldOpenExternalURLsPolicy::ShouldNotAllow;
}

void DocumentLoader::becomeMainResourceClient()
{
#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    if (m_contentFilter)
        m_contentFilter->startFilteringMainResource(*m_mainResource);
#endif
    m_mainResource->addClient(*this);
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

#if USE(QUICK_LOOK)

void DocumentLoader::previewResponseReceived(CachedResource& resource, const ResourceResponse& response)
{
    ASSERT_UNUSED(resource, m_mainResource == &resource);
    m_response = response;
}

void DocumentLoader::setPreviewConverter(RefPtr<PreviewConverter>&& previewConverter)
{
    m_previewConverter = WTFMove(previewConverter);
}

PreviewConverter* DocumentLoader::previewConverter() const
{
    return m_previewConverter.get();
}

#endif

void DocumentLoader::addConsoleMessage(MessageSource messageSource, MessageLevel messageLevel, const String& message, unsigned long requestIdentifier)
{
    static_cast<ScriptExecutionContext*>(m_frame->document())->addConsoleMessage(messageSource, messageLevel, message, requestIdentifier);
}

void DocumentLoader::sendCSPViolationReport(URL&& reportURL, Ref<FormData>&& report)
{
    PingLoader::sendViolationReport(*m_frame, WTFMove(reportURL), WTFMove(report), ViolationReportType::ContentSecurityPolicy);
}

void DocumentLoader::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&& eventInit)
{
    m_frame->document()->enqueueSecurityPolicyViolationEvent(WTFMove(eventInit));
}

#if ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
void DocumentLoader::dataReceivedThroughContentFilter(const SharedBuffer& buffer)
{
    dataReceived(buffer);
}

void DocumentLoader::cancelMainResourceLoadForContentFilter(const ResourceError& error)
{
    cancelMainResourceLoad(error);
}

ResourceError DocumentLoader::contentFilterDidBlock(ContentFilterUnblockHandler unblockHandler, String&& unblockRequestDeniedScript)
{
    return handleContentFilterDidBlock(unblockHandler, WTFMove(unblockRequestDeniedScript));
}

void DocumentLoader::handleProvisionalLoadFailureFromContentFilter(const URL& blockedPageURL, SubstituteData& substituteData)
{
    frameLoader()->load(FrameLoadRequest(*frame(), blockedPageURL, substituteData));
}
#endif // ENABLE(CONTENT_FILTERING) && !ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)

#if ENABLE(CONTENT_FILTERING)
ResourceError DocumentLoader::handleContentFilterDidBlock(ContentFilterUnblockHandler unblockHandler, String&& unblockRequestDeniedScript)
{
    unblockHandler.setUnreachableURL(documentURL());
    if (!unblockRequestDeniedScript.isEmpty() && frame()) {
        unblockHandler.wrapWithDecisionHandler([scriptController = WeakPtr { frame()->script() }, script = unblockRequestDeniedScript.isolatedCopy()](bool unblocked) {
            if (!unblocked && scriptController)
                scriptController->executeScriptIgnoringException(script);
        });
    }
    frameLoader()->client().contentFilterDidBlockLoad(WTFMove(unblockHandler));
    auto error = frameLoader()->blockedByContentFilterError(request());

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    m_blockedByContentFilter = true;
    m_blockedError = error;
#endif

    return error;
}

void DocumentLoader::handleContentFilterProvisionalLoadFailure(const URL& blockedPageURL, const SubstituteData& substituteData)
{
    frameLoader()->load(FrameLoadRequest(*frame(), blockedPageURL, substituteData));
}

bool DocumentLoader::contentFilterWillHandleProvisionalLoadFailure(const ResourceError& error)
{
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    return m_blockedByContentFilter && m_blockedError.errorCode() == error.errorCode() && m_blockedError.domain() == error.domain();
#else
    return m_contentFilter && m_contentFilter->willHandleProvisionalLoadFailure(error);
#endif
}

void DocumentLoader::contentFilterHandleProvisionalLoadFailure(const ResourceError& error)
{
#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)
    UNUSED_PARAM(error);
#else
    if (m_contentFilter)
        m_contentFilter->handleProvisionalLoadFailure(error);
#endif
}

#endif // ENABLE(CONTENT_FILTERING)

void DocumentLoader::setActiveContentRuleListActionPatterns(const HashMap<String, Vector<String>>& patterns)
{
    HashMap<String, Vector<UserContentURLPattern>> parsedPatternMap;

    for (auto& pair : patterns) {
        Vector<UserContentURLPattern> patternVector;
        patternVector.reserveInitialCapacity(pair.value.size());
        for (auto& patternString : pair.value) {
            UserContentURLPattern parsedPattern(patternString);
            if (parsedPattern.isValid())
                patternVector.uncheckedAppend(WTFMove(parsedPattern));
        }
        patternVector.shrinkToFit();
        parsedPatternMap.set(pair.key, WTFMove(patternVector));
    }

    m_activeContentRuleListActionPatterns = WTFMove(parsedPatternMap);
}

bool DocumentLoader::allowsActiveContentRuleListActionsForURL(const String& contentRuleListIdentifier, const URL& url) const
{
    for (const auto& pattern : m_activeContentRuleListActionPatterns.get(contentRuleListIdentifier)) {
        if (pattern.matches(url))
            return true;
    }
    return false;
}

} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
#undef IS_MAIN_FRAME
