/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include "Archive.h"
#include "ArchiveResourceCollection.h"
#include "CachedPage.h"
#include "CachedRawResource.h"
#include "ContentExtensionError.h"
#include "ContentRuleListResults.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginOpenerPolicy.h"
#include "CustomHeaderFields.h"
#include "DNS.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentInlines.h"
#include "DocumentParser.h"
#include "DocumentPrefetcher.h"
#include "DocumentQuirks.h"
#include "DocumentResourceLoader.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "DocumentWindow.h"
#include "DocumentWriter.h"
#include "ElementChildIteratorInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "ExtensionStyleSheets.h"
#include "FormSubmission.h"
#include "FrameInlines.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLObjectElement.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "HTTPStatusCodes.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "IconLoader.h"
#include "InspectorInstrumentation.h"
#include "IntegrityPolicy.h"
#include "LegacySchemeRegistry.h"
#include "LinkIconCollector.h"
#include "LinkIconType.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrameInlines.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "MixedContentChecker.h"
#include "NavigationNavigationType.h"
#include "NavigationScheduler.h"
#include "NetworkLoadMetrics.h"
#include "NetworkStorageSession.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "Performance.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include "PolicyChecker.h"
#include "ProgressTracker.h"
#include "ResourceLoadObserver.h"
#include "ResourceMonitor.h"
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
#include "ViolationReportType.h"
#include <wtf/Assertions.h>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/Scope.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
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

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#define PAGE_ID (m_frame && m_frame->pageID() ? m_frame->pageID()->toUInt64() : 0)
#define FRAME_ID (m_frame ? m_frame->frameID().toUInt64() : 0)
#define IS_MAIN_FRAME (m_frame ? m_frame->isMainFrame() : false)
#define DOCUMENTLOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", isMainFrame=%d] DocumentLoader::" fmt, this, PAGE_ID, FRAME_ID, IS_MAIN_FRAME, ##__VA_ARGS__)
#define DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(fmt, ...) RELEASE_LOG_FORWARDABLE(Network, fmt, PAGE_ID, FRAME_ID, IS_MAIN_FRAME, ##__VA_ARGS__)

namespace WebCore {

#if ENABLE(CONTENT_FILTERING)
static bool& contentFilterInDocumentLoader()
{
    static bool filter = false;
    RELEASE_ASSERT(isMainThread());
    return filter;
}
#endif

static void cancelAll(const ResourceLoaderMap& loaders)
{
    for (auto& loader : copyToVector(loaders))
        loader->cancel();
}

static void setAllDefersLoading(const ResourceLoaderMap& loaders, bool defers)
{
    for (auto& loader : copyToVector(loaders))
        loader->setDefersLoading(defers);
}

static HashMap<ScriptExecutionContextIdentifier, SingleThreadWeakPtr<DocumentLoader>>& scriptExecutionContextIdentifierToLoaderMap()
{
    static MainThreadNeverDestroyed<HashMap<ScriptExecutionContextIdentifier, SingleThreadWeakPtr<DocumentLoader>>> map;
    return map.get();
}

DocumentLoader* DocumentLoader::fromScriptExecutionContextIdentifier(ScriptExecutionContextIdentifier identifier)
{
    return scriptExecutionContextIdentifierToLoaderMap().get(identifier);
}

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DocumentLoader);

DocumentLoader::DocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
    : FrameDestructionObserver(nullptr)
    , m_cachedResourceLoader(CachedResourceLoader::create(this))
    , m_originalRequest(request)
    , m_substituteData(WTFMove(substituteData))
    , m_originalRequestCopy(request)
    , m_request(WTFMove(request))
    , m_substituteResourceDeliveryTimer(*this, &DocumentLoader::substituteResourceDeliveryTimerFired)
    , m_originalSubstituteDataWasValid(substituteData.isValid())
{
}

FrameLoader* DocumentLoader::frameLoader() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->loader();
}

RefPtr<FrameLoader> DocumentLoader::protectedFrameLoader() const
{
    return frameLoader();
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

    if (m_resultingClientId) {
        ASSERT(scriptExecutionContextIdentifierToLoaderMap().contains(*m_resultingClientId));
        scriptExecutionContextIdentifierToLoaderMap().remove(*m_resultingClientId);
    }

    if (auto createdCallback = std::exchange(m_whenDocumentIsCreatedCallback, { }))
        createdCallback(nullptr);
}

RefPtr<FragmentedSharedBuffer> DocumentLoader::mainResourceData() const
{
    if (m_substituteData.isValid())
        return m_substituteData.protectedContent()->copy();
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
    m_originalRequestCopy.setURL(URL { url });
    m_request.setURL(URL { url });
}

void DocumentLoader::setRequest(ResourceRequest&& req)
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

    m_request = WTFMove(req);
    if (shouldNotifyAboutProvisionalURLChange) {
        // Logging for <rdar://problem/54830233>.
        if (!frameLoader()->provisionalDocumentLoader())
            DOCUMENTLOADER_RELEASE_LOG("DocumentLoader::setRequest: With no provisional document loader");
        protectedFrameLoader()->protectedClient()->dispatchDidChangeProvisionalURL();
    }
}

void DocumentLoader::setMainDocumentError(const ResourceError& error)
{
    if (!error.isNull())
        DOCUMENTLOADER_RELEASE_LOG("setMainDocumentError: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

    m_mainDocumentError = error;    
    protectedFrameLoader()->protectedClient()->setMainDocumentError(this, error);
}

void DocumentLoader::mainReceivedError(const ResourceError& error, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    ASSERT(!error.isNull());

    if (auto createdCallback = std::exchange(m_whenDocumentIsCreatedCallback, { }))
        createdCallback(nullptr);

    if (!frameLoader())
        return;

    if (!error.isNull())
        DOCUMENTLOADER_RELEASE_LOG("mainReceivedError: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

    if (m_identifierForLoadWithoutResourceLoader) {
        ASSERT(!mainResourceLoader());
        protectedFrameLoader()->protectedClient()->dispatchDidFailLoading(this, *m_identifierForLoadWithoutResourceLoader, error);
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    setMainDocumentError(error);
    clearMainResourceLoader();
    protectedFrameLoader()->receivedMainResourceError(error, loadWillContinueInAnotherProcess);
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
    DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(DOCUMENTLOADER_STOPLOADING);

    RefPtr frame = m_frame.get();
    ASSERT(frame);
    if (!frame)
        return;

    Ref<DocumentLoader> protectedThis(*this);

    // In some rare cases, calling FrameLoader::stopLoading could cause isLoading() to return false.
    // (This can happen when there's a single XMLHttpRequest currently loading and stopLoading causes it
    // to stop loading. Because of this, we need to save it so we don't return early.
    bool loading = isLoading();

    if (m_committed) {
        // Attempt to stop the frame if the document loader is loading, or if it is done loading but
        // still  parsing. Failure to do so can cause a world leak.
        RefPtr document = frame->document();
        if (loading || document->parsing())
            frame->loader().stopLoading(UnloadEventPolicy::None);
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

    if (RefPtr document = this->document())
        document->suspendFontLoading();
    
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
    if (RefPtr frameLoader = this->frameLoader()) {
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
    if (RefPtr document = this->document())
        document->cancelParsing();
    
    stopLoadingSubresources();
    stopLoadingPlugIns();
    
    m_isStopping = false;
}

void DocumentLoader::commitIfReady()
{
    if (!m_committed) {
        m_committed = true;
        RefPtr protectedFrame { m_frame.get() };
        protectedFrameLoader()->commitProvisionalLoad();
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

void DocumentLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics& fetchMetrics, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    ASSERT(isMainThread());
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterNotifyFinished(resource))
        return;
#endif

    Box<NetworkLoadMetrics> metrics;
    if (RefPtr frameLoader = this->frameLoader()) {
        if (auto prefetchedMetrics = frameLoader->documentPrefetcher().takePrefetchedNetworkLoadMetrics(url())) {
            metrics = WTFMove(prefetchedMetrics);
            metrics->fromPrefetch = true;
        }
    }
    if (!metrics)
        metrics = Box<NetworkLoadMetrics>::create(fetchMetrics);

    if (RefPtr document = this->document()) {
        if (RefPtr window = document->window())
            window->protectedPerformance()->documentLoadFinished(*metrics);
    }

    ASSERT_UNUSED(resource, m_mainResource == &resource);
    ASSERT(m_mainResource);
    if (!m_mainResource->errorOccurred() && !m_mainResource->wasCanceled()) {
        finishedLoading();
        return;
    }

    if (m_request.cachePolicy() == ResourceRequestCachePolicy::ReturnCacheDataDontLoad && !m_mainResource->wasCanceled()) {
        protectedFrameLoader()->retryAfterFailedCacheOnlyMainResourceLoad();
        return;
    }

    if (!m_mainResource->resourceError().isNull())
        DOCUMENTLOADER_RELEASE_LOG("notifyFinished: canceling load (type=%d, code=%d)", static_cast<int>(m_mainResource->resourceError().type()), m_mainResource->resourceError().errorCode());

    mainReceivedError(m_mainResource->resourceError(), loadWillContinueInAnotherProcess);
}

void DocumentLoader::finishedLoading()
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!m_frame->page()->defersLoading() || protectedFrameLoader()->stateMachine().creatingInitialEmptyDocument() || InspectorInstrumentation::isDebuggerPaused(m_frame.get()));
#endif

    Ref<DocumentLoader> protectedThis(*this);

    if (m_identifierForLoadWithoutResourceLoader) {
        // A didFinishLoading delegate might try to cancel the load (despite it
        // being finished). Clear m_identifierForLoadWithoutResourceLoader
        // before calling dispatchDidFinishLoading so that we don't later try to
        // cancel the already-finished substitute load.
        NetworkLoadMetrics emptyMetrics;
        ResourceLoaderIdentifier identifier = *std::exchange(m_identifierForLoadWithoutResourceLoader, std::nullopt);
        protectedFrameLoader()->notifier().dispatchDidFinishLoading(this, identifier, emptyMetrics, nullptr);
    }

    maybeFinishLoadingMultipartContent();

    timing().markEndTime();
    
    commitIfReady();
    RefPtr frameLoader = this->frameLoader();
    if (!frameLoader)
        return;

    if (!maybeCreateArchive()) {
        // If this is an empty document, it will not have actually been created yet. Commit dummy data so that
        // DocumentWriter::begin() gets called and creates the Document.
        if (!m_gotFirstByte)
            commitData(SharedBuffer::create());

        frameLoader = this->frameLoader();
        if (!frameLoader)
            return;
        Ref frameLoaderClient = frameLoader->client();
        frameLoaderClient->finishedLoading(this);
        frameLoaderClient->loadStorageAccessQuirksIfNeeded();
    }

    m_writer.end();
    if (!m_mainDocumentError.isNull())
        return;
    clearMainResourceLoader();

    frameLoader = this->frameLoader();
    if (!frameLoader)
        return;
    if (!frameLoader->stateMachine().creatingInitialEmptyDocument())
        frameLoader->checkLoadComplete();
}

static bool isRedirectToGetAfterPost(const ResourceRequest& oldRequest, const ResourceRequest& newRequest)
{
    return oldRequest.httpMethod() == "POST"_s && newRequest.httpMethod() == "GET"_s;
}

bool DocumentLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST"_s)
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && m_originalRequest.httpMethod() == "POST"_s)
        return true;

    return false;
}

void DocumentLoader::handleSubstituteDataLoadNow()
{
    Ref<DocumentLoader> protectedThis = Ref { *this };
    
    if (m_substituteData.response().isRedirection()) {
        auto newRequest = m_request.redirectedRequest(m_substituteData.response(), true);
        auto substituteData = std::exchange(m_substituteData, { });
        auto callback = [protectedThis, newRequest] (auto&& request) mutable {
            if (request.isNull())
                return;
            protectedThis->loadMainResource(WTFMove(newRequest));
        };
        redirectReceived(WTFMove(newRequest), substituteData.response(), WTFMove(callback));
        return;
    }

    ResourceResponse response = m_substituteData.response();
    if (response.url().isEmpty())
        response = ResourceResponse(URL { m_request.url() }, String { m_substituteData.mimeType() }, m_substituteData.content()->size(), String { m_substituteData.textEncoding() });

#if ENABLE(CONTENT_EXTENSIONS)
    RefPtr frame = m_frame.get();
    RefPtr page = frame ? frame->page() : nullptr;
    RefPtr userContentProvider = frame ? frame->userContentProvider() : nullptr;
    if (page && userContentProvider) {
        // We intentionally do nothing with the results of this call.
        // We want the CSS to be loaded for us, but we ignore any attempt to block or upgrade the connection since there is no connection.
        userContentProvider->processContentRuleListsForLoad(*page, response.url(), frame->isMainFrame() ? ContentExtensions::ResourceType::TopDocument : ContentExtensions::ResourceType::ChildDocument, *this);
    }
#endif

    responseReceived(WTFMove(response), nullptr);
}

bool DocumentLoader::setControllingServiceWorkerRegistration(ServiceWorkerRegistrationData&& data)
{
    if (!m_loadingMainResource)
        return false;

    ASSERT(!m_gotFirstByte);
    m_serviceWorkerRegistrationData = makeUnique<ServiceWorkerRegistrationData>(WTFMove(data));
    return true;
}

void DocumentLoader::matchRegistration(const URL& url, SWClientConnection::RegistrationCallback&& callback)
{
    bool shouldTryLoadingThroughServiceWorker = m_canUseServiceWorkers && !frameLoader()->isReloadingFromOrigin() && m_frame->page() && url.protocolIsInHTTPFamily();
    if (!shouldTryLoadingThroughServiceWorker) {
        callback(std::nullopt);
        return;
    }

    RefPtr frame = m_frame.get();
    auto origin = (!frame->isMainFrame() && frame->document()) ? frame->protectedDocument()->topOrigin().data() : SecurityOriginData::fromURL(url);
    if (!ServiceWorkerProvider::singleton().protectedServiceWorkerConnection()->mayHaveServiceWorkerRegisteredForOrigin(origin)) {
        callback(std::nullopt);
        return;
    }

    Ref connection = ServiceWorkerProvider::singleton().serviceWorkerConnection();
    connection->matchRegistration(WTFMove(origin), url, WTFMove(callback));
}

void DocumentLoader::redirectReceived(CachedResource& resource, ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    ASSERT_UNUSED(resource, &resource == m_mainResource);
    redirectReceived(WTFMove(request), redirectResponse, WTFMove(completionHandler));
}

void DocumentLoader::redirectReceived(ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    if (m_serviceWorkerRegistrationData) {
        m_serviceWorkerRegistrationData = { };
        unregisterReservedServiceWorkerClient();
    }

    willSendRequest(WTFMove(request), redirectResponse, [completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }, this] (ResourceRequest&& request) mutable {
        ASSERT(!m_substituteData.isValid());
        if (request.isNull() || !m_mainDocumentError.isNull() || !m_frame) {
            completionHandler({ });
            return;
        }
        completionHandler(WTFMove(request));
    });
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
    if (!protectedFrameLoader()->checkIfFormActionAllowedByCSP(newRequest.url(), didReceiveRedirectResponse, redirectResponse.url())) {
        DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - form action not allowed by CSP");
        cancelMainResourceLoad(protectedFrameLoader()->cancelledError(newRequest));
        return completionHandler(WTFMove(newRequest));
    }

    RefPtr frame = m_frame.get();
    if (auto requester = m_triggeringAction.requester(); requester && requester->documentIdentifier) {
        if (RefPtr requestingDocument = Document::allDocumentsMap().get(requester->documentIdentifier); requestingDocument && requestingDocument->frame()) {
            if (frame && requestingDocument->isNavigationBlockedByThirdPartyIFrameRedirectBlocking(*frame, newRequest.url())) {
                DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - cross-site redirect of top frame triggered by third-party iframe");
                if (RefPtr document = frame->document()) {
                    auto message = makeString("Unsafe JavaScript attempt to initiate navigation for frame with URL '"_s
                        , document->url().string()
                        , "' from frame with URL '"_s
                        , requestingDocument->url().string()
                        , "'. The frame attempting navigation of the top-level window is cross-origin or untrusted and the user has never interacted with the frame."_s);
                    document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
                }
                cancelMainResourceLoad(protectedFrameLoader()->cancelledError(newRequest));
                return completionHandler(WTFMove(newRequest));
            }
        }
    }

    ASSERT(timing().startTime());
    if (didReceiveRedirectResponse) {
        if (newRequest.url().protocolIsAbout() || newRequest.url().protocolIsData()) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting URL scheme is not allowed");
            loadErrorDocument();
            if (frame && frame->document())
                frame->protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Not allowed to redirect to "_s, newRequest.url().stringCenterEllipsizedToLength(), " due to its scheme"_s));

            if (RefPtr frameLoader = this->frameLoader())
                cancelMainResourceLoad(frameLoader->blockedError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        Ref<SecurityOrigin> redirectingOrigin(SecurityOrigin::create(redirectResponse.url()));
        if (!redirectingOrigin.get().canDisplay(newRequest.url(), OriginAccessPatternsForWebProcess::singleton())) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting URL not allowed to display content from target");
            FrameLoader::reportLocalLoadFailed(frame.get(), newRequest.url().string());
            cancelMainResourceLoad(protectedFrameLoader()->cancelledError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
        if (!ResourceLoader::isPortAllowed(newRequest.url())) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting to a URL with a blocked port");
            if (frame)
                FrameLoader::reportBlockedLoadFailed(*frame, newRequest.url());
            cancelMainResourceLoad(protectedFrameLoader()->blockedError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
        if (isIPAddressDisallowed(newRequest.url())) {
            DOCUMENTLOADER_RELEASE_LOG("willSendRequest: canceling - redirecting to a URL with a disallowed IP address");
            if (frame)
                FrameLoader::reportBlockedLoadFailed(*frame, newRequest.url());
            cancelMainResourceLoad(protectedFrameLoader()->blockedError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
    }

    ASSERT(frame);

    RefPtr topFrame = frame->tree().top();

    RefPtr document = frame->document();
    ASSERT(document);
    
    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frame->isMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    FrameLoader::addSameSiteInfoToRequestIfNeeded(newRequest, document.get());

    if (!didReceiveRedirectResponse)
        protectedFrameLoader()->protectedClient()->dispatchWillChangeDocument(document->url(), newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == ResourceRequestCachePolicy::UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ResourceRequestCachePolicy::ReloadIgnoringCacheData);

    if (isRedirectToGetAfterPost(m_request, newRequest))
        newRequest.clearHTTPOrigin();

    if (topFrame && topFrame != frame.get()) {
        // We shouldn't check for mixed content against the current frame when navigating; we only need to be concerned with the ancestor frames.
        RefPtr parentFrame = frame->tree().parent();
        if (!parentFrame)
            parentFrame = frameLoader()->client().provisionalParentFrame();

        if (!parentFrame)
            return completionHandler(WTFMove(newRequest));

        if (MixedContentChecker::shouldBlockRequest(*parentFrame, newRequest.url())) {
            cancelMainResourceLoad(protectedFrameLoader()->cancelledError(newRequest));
            return completionHandler(WTFMove(newRequest));
        }
    }

    if (!newRequest.url().host().isEmpty() && SecurityOrigin::shouldIgnoreHost(newRequest.url())) {
        auto url = newRequest.url();
        url.removeHostAndPort();
        newRequest.setURL(WTFMove(url));
    }

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterWillSendRequest(newRequest, redirectResponse))
        return completionHandler(WTFMove(newRequest));
#endif

    setRequest(ResourceRequest { newRequest });

    if (!didReceiveRedirectResponse)
        return completionHandler(WTFMove(newRequest));

    auto navigationPolicyCompletionHandler = [this, protectedThis = Ref { *this }, frame, completionHandler = WTFMove(completionHandler)] (ResourceRequest&& request, WeakPtr<FormSubmission>&&, NavigationPolicyDecision navigationPolicyDecision) mutable {
        m_waitingForNavigationPolicy = false;
        switch (navigationPolicyDecision) {
        case NavigationPolicyDecision::IgnoreLoad:
        case NavigationPolicyDecision::LoadWillContinueInAnotherProcess:
            stopLoadingForPolicyChange(navigationPolicyDecision == NavigationPolicyDecision::LoadWillContinueInAnotherProcess ? LoadWillContinueInAnotherProcess::Yes : LoadWillContinueInAnotherProcess::No);
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
    RELEASE_ASSERT(!isBackForwardLoadType(policyChecker.loadType()) || frame->loader().history().provisionalItem());
    policyChecker.checkNavigationPolicy(WTFMove(newRequest), redirectResponse, WTFMove(navigationPolicyCompletionHandler));
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#process-a-navigate-fetch (Step 12.5.6)
std::optional<CrossOriginOpenerPolicyEnforcementResult> DocumentLoader::doCrossOriginOpenerHandlingOfResponse(const ResourceResponse& response)
{
    // COOP only applies to top-level browsing contexts.
    RefPtr frame = m_frame.get();
    if (!frame->isMainFrame())
        return std::nullopt;

    RefPtr document = frame->document();
    if (!document || !frame->document()->settings().crossOriginOpenerPolicyEnabled())
        return std::nullopt;

    URL openerURL;
    if (RefPtr openerFrame = dynamicDowncast<LocalFrame>(frame->opener()))
        openerURL = openerFrame->document() ? openerFrame->document()->url() : URL();

    auto currentCoopEnforcementResult = CrossOriginOpenerPolicyEnforcementResult::from(document->url(), document->securityOrigin(), document->crossOriginOpenerPolicy(), m_triggeringAction.requester(), openerURL);

    auto newCoopEnforcementResult = WebCore::doCrossOriginOpenerHandlingOfResponse(*document, response, m_triggeringAction.requester(), m_contentSecurityPolicy.get(), frame->effectiveSandboxFlags(), m_request.httpReferrer(), frameLoader()->stateMachine().isDisplayingInitialEmptyDocument(), currentCoopEnforcementResult);
    if (!newCoopEnforcementResult) {
        cancelMainResourceLoad(protectedFrameLoader()->cancelledError(m_request));
        return std::nullopt;
    }

    return newCoopEnforcementResult;
}

void DocumentLoader::setRedirectionAsSubstituteData(ResourceResponse&& response)
{
    ASSERT(response.isRedirection());
    m_substituteData = { SharedBuffer::create(), { }, WTFMove(response), SubstituteData::SessionHistoryVisibility::Visible };
}

bool DocumentLoader::tryLoadingSubstituteData()
{
    if (!m_substituteData.isValid() || !m_frame->page())
        return false;

    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Returning substitute data");
    m_identifierForLoadWithoutResourceLoader = ResourceLoaderIdentifier::generate();
    protectedFrameLoader()->notifier().assignIdentifierToInitialRequest(*m_identifierForLoadWithoutResourceLoader, this, m_request);
    protectedFrameLoader()->notifier().dispatchWillSendRequest(this, *m_identifierForLoadWithoutResourceLoader, m_request, ResourceResponse(), nullptr);

    if (!m_deferMainResourceDataLoad || protectedFrameLoader()->loadsSynchronously())
        handleSubstituteDataLoadNow();
    else {
        auto loadData = [weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->handleSubstituteDataLoadNow();
        };

#if USE(COCOA_EVENT_LOOP)
        RunLoop::dispatch(*m_frame->page()->scheduledRunLoopPairs(), WTFMove(loadData));
#else
        RunLoop::currentSingleton().dispatch(WTFMove(loadData));
#endif
    }

    return true;
}

void DocumentLoader::stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    Ref<DocumentLoader> protectedThis { *this };
    InspectorInstrumentation::continueAfterXFrameOptionsDenied(*protectedFrame(), identifier, *this, response);

    loadErrorDocument();

    // The load event might have detached this frame. In that case, the load will already have been cancelled during detach.
    if (RefPtr frameLoader = this->frameLoader())
        cancelMainResourceLoad(frameLoader->cancelledError(m_request));
}

static URL microsoftTeamsRedirectURL()
{
    return URL { "https://www.microsoft.com/en-us/microsoft-365/microsoft-teams/"_str };
}

bool DocumentLoader::shouldClearContentSecurityPolicyForResponse(const ResourceResponse& response) const
{
    return response.httpHeaderField(HTTPHeaderName::ContentSecurityPolicy).isNull() && !m_isLoadingMultipartContent;
}

void DocumentLoader::responseReceived(const CachedResource& resource, const ResourceResponse& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT_UNUSED(resource, m_mainResource == &resource);

    RefPtr frame = m_frame.get();
    if (shouldClearContentSecurityPolicyForResponse(response))
        m_contentSecurityPolicy = nullptr;
    else {
        // FIXME(294912): Clean up use of bare pointers for ReportingClient
        ReportingClient* reportingClient = nullptr;
        if (frame && frame->document())
            reportingClient = frame->document();

        if (!m_contentSecurityPolicy)
            m_contentSecurityPolicy = makeUnique<ContentSecurityPolicy>(URL { response.url() }, nullptr, reportingClient);
        m_contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, m_request.httpReferrer(), ContentSecurityPolicy::ReportParsingErrors::No);
    }
    if (frame && frame->document() && frame->document()->settings().crossOriginOpenerPolicyEnabled())
        m_responseCOOP = obtainCrossOriginOpenerPolicy(response);

    // https://w3c.github.io/webappsec-subresource-integrity/#parse-integrity-policy-headers
    m_integrityPolicy = processIntegrityPolicy(response, HTTPHeaderName::IntegrityPolicy);
    m_integrityPolicyReportOnly = processIntegrityPolicy(response, HTTPHeaderName::IntegrityPolicyReportOnly);

    if (frame && frame->settings().clearSiteDataHTTPHeaderEnabled())
        m_responseClearSiteDataValues = parseClearSiteDataHeader(response);

    // FIXME(218779): Remove this quirk once microsoft.com completes their login flow redesign.
    if (frame && frame->document()) {
        Ref document = *frame->document();
        if (Quirks::isMicrosoftTeamsRedirectURL(response.url())) {
            auto firstPartyDomain = RegistrableDomain(response.url());
            if (auto loginDomains = NetworkStorageSession::subResourceDomainsInNeedOfStorageAccessForFirstParty(firstPartyDomain)) {
                if (!Quirks::hasStorageAccessForAllLoginDomains(*loginDomains, firstPartyDomain)) {
                    frame->protectedNavigationScheduler()->scheduleRedirect(document, 0, microsoftTeamsRedirectURL(), IsMetaRefresh::No);
                    completionHandler();
                    return;
                }
            }
        }
    }

    if (m_canUseServiceWorkers && response.source() == ResourceResponse::Source::MemoryCache) {
        matchRegistration(response.url(), [this, protectedThis = Ref { *this }, response = ResourceResponse { response }, completionHandler = WTFMove(completionHandler)](auto&& registrationData) mutable {
            if (!m_mainDocumentError.isNull() || !m_frame) {
                completionHandler();
                return;
            }
            if (registrationData)
                m_serviceWorkerRegistrationData = makeUnique<ServiceWorkerRegistrationData>(WTFMove(*registrationData));
            responseReceived(WTFMove(response), WTFMove(completionHandler));
        });
        return;
    }
    responseReceived(ResourceResponse { response }, WTFMove(completionHandler));
}

void DocumentLoader::responseReceived(ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(response.certificateInfo());
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(completionHandler));

#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterResponseReceived(response))
        return;
#endif

    Ref<DocumentLoader> protectedThis(*this);

    ASSERT(m_identifierForLoadWithoutResourceLoader || m_mainResource);
    ResourceLoaderIdentifier identifier = m_identifierForLoadWithoutResourceLoader ? *m_identifierForLoadWithoutResourceLoader : *m_mainResource->resourceLoaderIdentifier();

    if (m_substituteData.isValid() || !platformStrategies()->loaderStrategy()->havePerformedSecurityChecks(response)) {
        auto url = response.url();
        RefPtr frame = m_frame.get();
        // FIXME(294912): Clean up use of bare pointers for ReportingClient
        ReportingClient* reportingClient = nullptr;
        if (frame && frame->document())
            reportingClient = frame->document();
        ContentSecurityPolicy contentSecurityPolicy(URL { url }, this, reportingClient);
        contentSecurityPolicy.didReceiveHeaders(ContentSecurityPolicyResponseHeaders { response }, m_request.httpReferrer());
        if (frame && !contentSecurityPolicy.allowFrameAncestors(*frame, url)) {
            stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(identifier, response);
            return;
        }

        if (frame && !contentSecurityPolicy.overridesXFrameOptions()) {
            String frameOptions = response.httpHeaderFields().get(HTTPHeaderName::XFrameOptions);
            if (!frameOptions.isNull()) {
                if (protectedFrameLoader()->shouldInterruptLoadForXFrameOptions(frameOptions, url, identifier)) {
                    auto message = makeString("Refused to display '"_s, url.stringCenterEllipsizedToLength(), "' in a frame because it set 'X-Frame-Options' to '"_s, frameOptions, "'."_s);
                    frame->protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, identifier.toUInt64());
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
        setupForMultipartReplace();
        m_mainResource->clear();
    } else if (response.isMultipart())
        m_isLoadingMultipartContent = true;

    m_response = WTFMove(response);

    if (m_identifierForLoadWithoutResourceLoader) {
        RefPtr frameLoader = this->frameLoader();
        if (m_mainResource && m_mainResource->wasRedirected()) {
            ASSERT(m_mainResource->status() == CachedResource::Status::Cached);
            if (frameLoader)
                frameLoader->protectedClient()->dispatchDidReceiveServerRedirectForProvisionalLoad();
        }
        addResponse(m_response);
        if (frameLoader)
            frameLoader->notifier().dispatchDidReceiveResponse(this, *m_identifierForLoadWithoutResourceLoader, m_response, 0);
    }

    ASSERT(!m_waitingForContentPolicy);
    ASSERT(frameLoader());
    m_waitingForContentPolicy = true;

    // Always show content with valid substitute data.
    if (m_substituteData.isValid()) {
        continueAfterContentPolicy(PolicyAction::Use);
        return;
    }

    RefPtr frame = m_frame.get();
#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    if (frame && frame->settings().forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory"_s) {
        continueAfterContentPolicy(PolicyAction::Use);
        return;
    }
#endif

    if (!frame) {
        DOCUMENTLOADER_RELEASE_LOG("responseReceived by DocumentLoader with null frame");
        return;
    }

    RefPtr<SubresourceLoader> mainResourceLoader = this->mainResourceLoader();
    if (mainResourceLoader)
        mainResourceLoader->markInAsyncResponsePolicyCheck();

    protectedFrameLoader()->checkContentPolicy(m_response, [this, protectedThis = Ref { *this }, mainResourceLoader = WTFMove(mainResourceLoader),
        completionHandler = completionHandlerCaller.release()] (PolicyAction policy) mutable {
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
    String mimeType = m_response.mimeType();
    if (mimeType.isNull() || !MIMETypeRegistry::isWebArchiveMIMEType(mimeType))
        return false;

#if USE(QUICK_LOOK)
    if (isQuickLookPreviewURL(m_response.url()))
        return false;
#endif

    if (m_substituteData.isValid())
        return false;

    if (!LegacySchemeRegistry::shouldTreatURLSchemeAsLocal(m_request.url().protocol()))
        return true;

#if ENABLE(WEB_ARCHIVE)
    // On purpose of maintaining existing tests.
    bool alwaysAllowLocalWebArchive = frame()->mainFrame().settings().alwaysAllowLocalWebarchive();
#else
    bool alwaysAllowLocalWebArchive { false };
#endif
    if (!frame() || (frame()->isMainFrame() && allowsWebArchiveForMainFrame()) || alwaysAllowLocalWebArchive)
        return false;

    return true;
}

// Prevent data URIs from loading as the main frame unless the result of user action.
bool DocumentLoader::disallowDataRequest() const
{
    if (!m_response.url().protocolIsData())
        return false;

    if (!frame() || !frame()->isMainFrame() || allowsDataURLsForMainFrame() || frame()->settings().allowTopNavigationToDataURLs())
        return false;

    if (RefPtr currentDocument = frame()->document()) {
        ResourceLoaderIdentifier identifier = m_identifierForLoadWithoutResourceLoader ? *m_identifierForLoadWithoutResourceLoader : *m_mainResource->resourceLoaderIdentifier();
        currentDocument->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Not allowed to navigate top frame to data URL '"_s, m_response.url().stringCenterEllipsizedToLength(), "'."_s), identifier.toUInt64());
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

    RefPtr frame = m_frame.get();
    if (!frame) {
        DOCUMENTLOADER_RELEASE_LOG("continueAfterContentPolicy: policyAction=%" PUBLIC_LOG_STRING " received by DocumentLoader with null frame", toString(policy).characters());
        return;
    }

    switch (policy) {
    case PolicyAction::Use: {
        if (!protectedFrameLoader()->protectedClient()->canShowMIMEType(m_response.mimeType()) || disallowWebArchive() || disallowDataRequest()) {
            protectedFrameLoader()->policyChecker().cannotShowMIMEType(m_response);
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
            mainReceivedError(platformStrategies()->loaderStrategy()->cannotShowURLError(m_request));
            return;
        }

        if (RefPtr mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyDownload(*frame, *mainResourceLoader->identifier(), *this, m_response);

        if (!frame->effectiveSandboxFlags().contains(SandboxFlag::Downloads)) {
            // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
            // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
            protectedFrameLoader()->setOriginalURLForDownloadRequest(m_request);

            if (m_request.url().protocolIsData()) {
                // We decode data URL internally, there is no resource load to convert.
                protectedFrameLoader()->protectedClient()->startDownload(m_request);
            } else
                protectedFrameLoader()->protectedClient()->convertMainResourceLoadToDownload(this, m_request, m_response);
        } else if (RefPtr document = frame->document())
            document->addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Not allowed to download due to sandboxing"_s);

        // The main resource might be loading from the memory cache, or its loader might have gone missing.
        if (RefPtr loader = mainResourceLoader()) {
            loader->didFail(interruptedForPolicyChangeError());
            return;
        }

        // We must stop loading even if there is no main resource loader. Otherwise, we might remain
        // the client of a CachedRawResource that will continue to send us data.
        stopLoadingForPolicyChange();
        return;
    }
    case PolicyAction::LoadWillContinueInAnotherProcess:
        ASSERT_NOT_REACHED();
#if !ASSERT_ENABLED
        [[fallthrough]];
#endif
    case PolicyAction::Ignore:
        if (RefPtr mainResourceLoader = this->mainResourceLoader())
            InspectorInstrumentation::continueWithPolicyIgnore(*frame, *mainResourceLoader->identifier(), *this, m_response);
        stopLoadingForPolicyChange();
        return;
    }

    if (m_response.isInHTTPFamily()) {
        int status = m_response.httpStatusCode(); // Status may be zero when loading substitute data, in particular from a WebArchive.
        if (status) {
            if (status < httpStatus200OK || status >= httpStatus300MultipleChoices) {
                if (RefPtr owner = dynamicDowncast<HTMLObjectElement>(frame->ownerElement())) {
                    owner->renderFallbackContent();
                    // object elements are no longer rendered after we fallback, so don't
                    // keep trying to process data from their load
                    cancelMainResourceLoad(protectedFrameLoader()->cancelledError(m_request));
                }
            } else if (status == httpStatus204NoContent || status == httpStatus205ResetContent) {
                // 204/205 responses should abort navigation without changing the document.
                stopLoadingForPolicyChange();
                return;
            }
        }
    }

    if (!isStopping() && m_substituteData.isValid() && isLoadingMainResource()) {
        RefPtr content = m_substituteData.content();
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
    RefPtr protectedFrame { m_frame.get() };
    Ref protectedThis { *this };

    commitIfReady();
    RefPtr frameLoader = this->frameLoader();
    if (!frameLoader)
        return;
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (ArchiveFactory::isArchiveMIMEType(response().mimeType()))
        return;
#endif
    Ref client = frameLoader->client();
    client->committedLoad(this, data);

    if (isMultipartReplacingLoad())
        client->didReplaceMultipartContent();
}

ResourceError DocumentLoader::interruptedForPolicyChangeError() const
{
    if (!frameLoader()) {
        ResourceError error;
        error.setType(ResourceError::Type::Cancellation);
        return error;
    }

    auto error = platformStrategies()->loaderStrategy()->interruptedForPolicyChangeError(request());
    error.setType(ResourceError::Type::Cancellation);
    return error;
}

void DocumentLoader::stopLoadingForPolicyChange(LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    cancelMainResourceLoad(interruptedForPolicyChangeError(), loadWillContinueInAnotherProcess);
}

// https://w3c.github.io/ServiceWorker/#control-and-use-window-client
static inline bool shouldUseActiveServiceWorkerFromParent(const Document& document, const Document& parent)
{
    return !document.url().protocolIsInHTTPFamily() && !document.securityOrigin().isOpaque() && parent.protectedSecurityOrigin()->isSameOriginDomain(document.protectedSecurityOrigin());
}

#if ENABLE(CONTENT_EXTENSIONS)
static inline bool shouldEnableResourceMonitor(const Frame& frame)
{
    if (frame.isMainFrame())
        return false;

    return frame.settings().iFrameResourceMonitoringEnabled();
}
#endif

void DocumentLoader::commitData(const SharedBuffer& data)
{
    if (!m_gotFirstByte) {
        m_gotFirstByte = true;
        bool hasBegun = m_writer.begin(documentURL(), false, nullptr, m_resultingClientId, &triggeringAction());
        if (!hasBegun)
            return;

        m_writer.setDocumentWasLoadedAsPartOfNavigation();

        RefPtr frame = m_frame.get();
        RefPtr documentOrNull = frame ? frame->document() : nullptr;

        auto scope = makeScopeExit([this, protectedThis = Ref { *this }, documentOrNull] {
            if (auto createdCallback = std::exchange(m_whenDocumentIsCreatedCallback, { }))
                createdCallback(isInFinishedLoadingOfEmptyDocument() ? nullptr : documentOrNull.get());
        });

        if (!documentOrNull)
            return;
        Ref document = *documentOrNull;
        ASSERT(frame);

#if ENABLE(CONTENT_EXTENSIONS)
        if (shouldEnableResourceMonitor(*frame)) {
            URL url = documentURL();

            if (!url.isEmpty() && url.protocolIsInHTTPFamily())
                document->protectedResourceMonitor()->setDocumentURL(WTFMove(url));
        }
#endif

        if (SecurityPolicy::allowSubstituteDataAccessToLocal() && m_originalSubstituteDataWasValid) {
            // If this document was loaded with substituteData, then the document can
            // load local resources. See https://bugs.webkit.org/show_bug.cgi?id=16756
            // and https://bugs.webkit.org/show_bug.cgi?id=19760 for further
            // discussion.
            document->protectedSecurityOrigin()->grantLoadLocalResources();
        }

        if (protectedFrameLoader()->stateMachine().creatingInitialEmptyDocument())
            return;

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
        if (RefPtr archive = m_archive; archive && archive->shouldOverrideBaseURL())
            document->setBaseURLOverride(archive->mainResource()->url());
#endif
        if (m_canUseServiceWorkers) {
            if (!document->securityOrigin().isOpaque()) {
                if (m_serviceWorkerRegistrationData && m_serviceWorkerRegistrationData->activeWorker) {
                    document->setActiveServiceWorker(ServiceWorker::getOrCreate(document, WTFMove(m_serviceWorkerRegistrationData->activeWorker.value())));
                    m_serviceWorkerRegistrationData = { };
                } else if (RefPtr parent = document->parentDocument()) {
                    if (shouldUseActiveServiceWorkerFromParent(document, *parent))
                        document->setActiveServiceWorker(parent->activeServiceWorker());
                }
            } else if (m_resultingClientId) {
                // In case document has an opaque origin, say due to sandboxing, we should have created a new context, let's create a new identifier instead.
                if (document->securityOrigin().isOpaque())
                    document->createNewIdentifier();
            }

            if (m_frame->document()->activeServiceWorker() || document->url().protocolIsInHTTPFamily() || (document->page() && document->page()->isServiceWorkerPage()) || (document->parentDocument() && shouldUseActiveServiceWorkerFromParent(document, *document->protectedParentDocument())))
                document->setServiceWorkerConnection(&ServiceWorkerProvider::singleton().serviceWorkerConnection());

            if (m_resultingClientId) {
                if (*m_resultingClientId != document->identifier())
                    unregisterReservedServiceWorkerClient();
                scriptExecutionContextIdentifierToLoaderMap().remove(*m_resultingClientId);
                m_resultingClientId = std::nullopt;
            }
        }
        // Call receivedFirstData() exactly once per load. We should only reach this point multiple times
        // for multipart loads, and FrameLoader::isMultipartReplacing() will be true after the first time.
        if (!isMultipartReplacingLoad())
            protectedFrameLoader()->receivedFirstData();

        // The load could be canceled under receivedFirstData(), which makes delegate calls and even sometimes dispatches DOM events.
        if (!isLoading())
            return;

        if (RefPtr window = document->window()) {
            window->prewarmLocalStorageIfNecessary();

            if (m_mainResource) {
                auto* metrics = m_response.deprecatedNetworkLoadMetricsOrNull();
                NetworkLoadMetrics finalMetrics = metrics ? *metrics : NetworkLoadMetrics::emptyMetrics();
                if (RefPtr frameLoader = this->frameLoader())
                    finalMetrics.fromPrefetch = frameLoader->documentPrefetcher().wasPrefetched(url());
                window->protectedPerformance()->addNavigationTiming(*this, document, *m_mainResource, timing(), finalMetrics);
            }
        }

        DocumentWriter::IsEncodingUserChosen userChosen;
        String encoding;
        if (overrideEncoding().isNull()) {
            userChosen = DocumentWriter::IsEncodingUserChosen::No;
            encoding = response().textEncodingName();
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
            if (RefPtr archive = m_archive; archive && archive->shouldUseMainResourceEncoding())
                encoding = archive->mainResource()->textEncoding();
#endif
        } else {
            userChosen = DocumentWriter::IsEncodingUserChosen::Yes;
            encoding = overrideEncoding();
        }

        m_writer.setEncoding(encoding, userChosen);
    }

#if ENABLE(CONTENT_EXTENSIONS)
    if (!m_pendingNamedContentExtensionStyleSheets.isEmpty() || !m_pendingContentExtensionDisplayNoneSelectors.isEmpty()) {
        auto& extensionStyleSheets = m_frame->protectedDocument()->extensionStyleSheets();
        for (auto& pendingStyleSheet : m_pendingNamedContentExtensionStyleSheets)
            extensionStyleSheets.maybeAddContentExtensionSheet(pendingStyleSheet.key, Ref { *pendingStyleSheet.value });
        for (auto& pendingSelectorEntry : m_pendingContentExtensionDisplayNoneSelectors) {
            for (const auto& pendingSelector : pendingSelectorEntry.value)
                extensionStyleSheets.addDisplayNoneSelector(pendingSelectorEntry.key, pendingSelector.first, pendingSelector.second);
        }
        m_pendingNamedContentExtensionStyleSheets.clear();
        m_pendingContentExtensionDisplayNoneSelectors.clear();
    }
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
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter && !m_contentFilter->continueAfterDataReceived(buffer, ContentFilter::FromDocumentLoader::Yes))
        return;
#endif

    ASSERT(!buffer.span().empty());
    ASSERT(!m_response.isNull());

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!mainResourceLoader() || !mainResourceLoader()->defersLoading());
#endif

    if (m_identifierForLoadWithoutResourceLoader)
        protectedFrameLoader()->notifier().dispatchDidReceiveData(this, *m_identifierForLoadWithoutResourceLoader, &buffer, buffer.size(), -1);

    if (!isMultipartReplacingLoad())
        commitLoad(buffer);
}

void DocumentLoader::setupForMultipartReplace()
{
    if (!mainResourceData())
        return;

    protectedFrameLoader()->protectedClient()->willReplaceMultipartContent();
    
    maybeFinishLoadingMultipartContent();
    maybeCreateArchive();
    m_writer.end();
    protectedFrameLoader()->setMultipartReplacing();
    m_gotFirstByte = false;

    unregisterReservedServiceWorkerClient();
    if (m_resultingClientId) {
        scriptExecutionContextIdentifierToLoaderMap().remove(*m_resultingClientId);
        m_resultingClientId = std::nullopt;
    }

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
    m_frame->protectedDocument()->protectedWindow()->finishedLoading();
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
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    if (m_legacyOverflowScrollingTouchPolicy == LegacyOverflowScrollingTouchPolicy::Disable)
        m_frame->settings().setLegacyOverflowScrollingTouchEnabled(false);
#endif
#if ENABLE(TEXT_AUTOSIZING)
    m_frame->settings().setIdempotentModeAutosizingOnlyHonorsPercentages(m_idempotentModeAutosizingOnlyHonorsPercentages);
#endif

    if (m_pushAndNotificationsEnabledPolicy != PushAndNotificationsEnabledPolicy::UseGlobalPolicy) {
        bool enabled = m_pushAndNotificationsEnabledPolicy == PushAndNotificationsEnabledPolicy::Yes;
        m_frame->settings().setPushAPIEnabled(enabled);
#if ENABLE(NOTIFICATIONS)
        m_frame->settings().setNotificationsEnabled(enabled);
#endif
#if ENABLE(NOTIFICATION_EVENT)
        m_frame->settings().setNotificationEventEnabled(enabled);
#endif
#if PLATFORM(IOS)
        m_frame->settings().setAppBadgeEnabled(enabled);
#endif
    }

    if (m_inlineMediaPlaybackPolicy != InlineMediaPlaybackPolicy::Default)
        m_frame->settings().setInlineMediaPlaybackRequiresPlaysInlineAttribute(m_inlineMediaPlaybackPolicy == InlineMediaPlaybackPolicy::RequiresPlaysInlineAttribute);
}

ColorSchemePreference DocumentLoader::colorSchemePreference() const
{
    return m_colorSchemePreference;
}

void DocumentLoader::attachToFrame(LocalFrame& frame)
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
    DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(DOCUMENTLOADER_ATTACHTOFRAME);
}

void DocumentLoader::detachFromFrame(LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(DOCUMENTLOADER_DETACHFROMFRAME);

    RefPtr frame = m_frame.get();
#if ASSERT_ENABLED
    if (m_hasEverBeenAttached)
        ASSERT_WITH_MESSAGE(frame, "detachFromFrame() is being called on a DocumentLoader twice without an attachToFrame() inbetween");
    else
        ASSERT_WITH_MESSAGE(frame, "detachFromFrame() is being called on a DocumentLoader that has never attached to any Frame");
#endif
    Ref protectedThis { *this };

    // It never makes sense to have a document loader that is detached from its
    // frame have any loads active, so kill all the loads.
    stopLoading();
    if (m_mainResource && m_mainResource->hasClient(*this))
        m_mainResource->removeClient(*this);
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    cancelPolicyCheckIfNeeded();

    // cancelPolicyCheckIfNeeded can clear m_frame if the policy check
    // is stopped, resulting in a recursive call into this detachFromFrame.
    // If m_frame is nullptr after cancelPolicyCheckIfNeeded, our work is
    // already done so just return.
    frame = m_frame.get();
    if (!frame)
        return;

    if (auto navigationID = std::exchange(m_navigationID, { }))
        frame->loader().client().documentLoaderDetached(*navigationID, loadWillContinueInAnotherProcess);

    InspectorInstrumentation::loaderDetachedFromFrame(*frame, *this);

    observeFrame(nullptr);
}

void DocumentLoader::setNavigationID(NavigationIdentifier navigationID)
{
    m_navigationID = navigationID;
}

void DocumentLoader::clearMainResourceLoader()
{
    m_loadingMainResource = false;
    m_isContinuingLoadAfterProvisionalLoadStarted = false;

    RefPtr frameLoader = this->frameLoader();

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
    auto notifyIfUnableToLoad = makeScopeExit([this, protectedThis = Ref { *this }, &isLoading] {
        if (!isLoading || m_finishedLoadingApplicationManifest)
            notifyFinishedLoadingApplicationManifest();
    });

    if (isLoading)
        return;

    RefPtr document = this->document();
    if (!document)
        return;

    if (!document->isTopDocument())
        return;

    if (document->url().isEmpty() || document->url().protocolIsAbout())
        return;

    RefPtr head = document->head();
    if (!head)
        return;

    URL manifestURL;
    bool useCredentials = false;
    for (Ref link : childrenOfType<HTMLLinkElement>(*head)) {
        if (!link->isApplicationManifest())
            continue;

        auto href = link->href();
        if (href.isEmpty() || !href.isValid())
            continue;

        if (!link->mediaAttributeMatches())
            continue;

        manifestURL = href;
        useCredentials = equalLettersIgnoringASCIICase(link->attributeWithoutSynchronization(HTMLNames::crossoriginAttr), "use-credentials"_s);
        break;
    }

    if (manifestURL.isEmpty() || !manifestURL.isValid())
        return;

    Ref applicationManifestLoader = ApplicationManifestLoader::create(*this, manifestURL, useCredentials);
    m_applicationManifestLoader = applicationManifestLoader.copyRef();

    isLoading = applicationManifestLoader->startLoading();
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
    std::optional<ApplicationManifest> manifest = m_applicationManifestLoader ? Ref { *m_applicationManifestLoader }->processManifest() : std::nullopt;
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
        Ref document = *m_frame->document();
        if ((isLoadingMainResource() || !document->loadEventFinished()) && isLoading())
            return true;
        if (m_cachedResourceLoader->requestCount())
            return true;
        if (document->isDelayingLoadEvent())
            return true;
        if (document->processingLoadEvent())
            return true;
        if (document->hasActiveParser())
            return true;
        RefPtr scriptableParser = document->scriptableDocumentParser();
        if (scriptableParser && scriptableParser->hasScriptsWaitingForStylesheets())
            return true;
    }
    return protectedFrameLoader()->subframeIsLoading();
}

bool DocumentLoader::maybeCreateArchive()
{
#if !ENABLE(WEB_ARCHIVE) && !ENABLE(MHTML)
    return false;
#else
    // Give the archive machinery a crack at this document. If the MIME type is not an archive type, it will return 0.
    RefPtr archive = ArchiveFactory::create(m_response.url(), mainResourceData().get(), m_response.mimeType());
    m_archive = archive.copyRef();
    if (!archive)
        return false;
    
    addAllArchiveResources(*archive);
    ASSERT(archive->mainResource());
    Ref mainResource = *archive->mainResource();
    Ref parsedArchiveData = mainResource->protectedData()->makeContiguous();
    m_parsedArchiveData = parsedArchiveData.copyRef();
    m_writer.setMIMEType(mainResource->mimeType());

    ASSERT(m_frame->document());
    commitData(parsedArchiveData);
    return true;
#endif
}

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)

void DocumentLoader::setArchive(Ref<Archive>&& archive)
{
    m_archive = archive.copyRef();
    addAllArchiveResources(archive);
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

RefPtr<ArchiveResource> DocumentLoader::archiveResourceForURL(const URL& url) const
{
    if (!m_archiveResourceCollection)
        return nullptr;
    RefPtr resource = m_archiveResourceCollection->archiveResourceForURL(url);
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

    RefPtr data = resource->resourceBuffer();
    if (!data)
        return nullptr;

    return ArchiveResource::create(data.get(), url, resource->response());
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
    if (RefPtr resource = archiveResourceForURL(request.url())) {
        scheduleSubstituteResourceLoad(loader, *resource);
        return true;
    }

    RefPtr archive = m_archive;
    if (!archive)
        return false;

#if ENABLE(WEB_ARCHIVE)
    // The idea of WebArchiveDebugMode is that we should fail instead of trying to fetch from the network.
    // Returning true ensures the caller will not try to fetch from the network.
    if (m_frame->settings().webArchiveDebugModeEnabled() && responseMIMEType() == "application/x-webarchive"_s)
        return true;
#endif

    // If we want to load from the archive only, then we should always return true so that the caller
    // does not try to fetch from the network.
    return archive->shouldLoadFromArchiveOnly();
}

#endif

void DocumentLoader::scheduleSubstituteResourceLoad(ResourceLoader& loader, SubstituteResource& resource)
{
    ASSERT(!loader.options().serviceWorkerRegistrationIdentifier);
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

    protectedFrameLoader()->willChangeTitle(this);
    m_pageTitle = title;
    if (RefPtr frameLoader = this->frameLoader())
        frameLoader->didChangeTitle(this);
}

URL DocumentLoader::urlForHistory() const
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates
    // for unreachable URLs, because these can't be stored in history.
    if (m_substituteData.isValid() && m_substituteData.shouldRevealToSessionHistory() != SubstituteData::SessionHistoryVisibility::Visible)
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
    if (RefPtr archive = m_archive; url.isEmpty() && archive && archive->shouldUseMainResourceURL())
        url = archive->mainResource()->url();
#endif
    if (url.isEmpty())
        url = m_request.url();
    if (url.isEmpty())
        url = m_response.url();
    return url;
}

#if PLATFORM(IOS_FAMILY)

// FIXME: This method seems to violate the encapsulation of this class.
void DocumentLoader::setResponseMIMEType(const String& responseMIMEType)
{
    m_response.setMimeType(String { responseMIMEType });
}

#endif

void DocumentLoader::setDefersLoading(bool defers)
{
    // Multiple frames may be loading the same main resource simultaneously. If deferral state changes,
    // each frame's DocumentLoader will try to send a setDefersLoading() to the same underlying ResourceLoader. Ensure only
    // the "owning" DocumentLoader does so, as setDefersLoading() is not resilient to setting the same value repeatedly.
    if (RefPtr loader = mainResourceLoader(); loader && loader->documentLoader() == this)
        loader->setDefersLoading(defers);

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
    for (auto& loader : copyToVector(m_subresourceLoaders)) {
        if (CachedResourceHandle cachedResource = loader->cachedResource()) {
            // Don't cancel loaders for prefetch resources, as they need to survive navigation.
            if (cachedResource->options().cachingPolicy == CachingPolicy::AllowCachingMainResourcePrefetch) {
                m_subresourceLoaders.remove(loader);
                continue;
            }
        }
        loader->cancel();
    }
    ASSERT(m_subresourceLoaders.isEmpty());
}

void DocumentLoader::addSubresourceLoader(SubresourceLoader& loader)
{
    // The main resource's underlying ResourceLoader will ask to be added here.
    // It is much simpler to handle special casing of main resource loads if we don't
    // let it be added. In the main resource load case, mainResourceLoader()
    // will still be null at this point, but m_gotFirstByte should be false here if and only
    // if we are just starting the main resource load.
    if (!m_gotFirstByte)
        return;

    ASSERT(!m_subresourceLoaders.contains(&loader));
    ASSERT(!mainResourceLoader() || mainResourceLoader() != &loader);

#if ASSERT_ENABLED
    if (document()) {
        switch (document()->backForwardCacheState()) {
        case Document::NotInBackForwardCache:
            break;
        case Document::AboutToEnterBackForwardCache: {
            // A page about to enter the BackForwardCache should only be able to start ping loads.
            auto* cachedResource = loader.cachedResource();
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

    m_subresourceLoaders.add(&loader);
}

void DocumentLoader::removeSubresourceLoader(LoadCompletionType type, SubresourceLoader& loader)
{
    if (!m_subresourceLoaders.remove(&loader))
        return;
    checkLoadComplete();
    if (RefPtr frame = m_frame.get())
        frame->loader().subresourceLoadDone(type);
}

void DocumentLoader::addPlugInStreamLoader(ResourceLoader& loader)
{
    ASSERT(!m_plugInStreamLoaders.contains(&loader));

    m_plugInStreamLoaders.add(&loader);
}

void DocumentLoader::removePlugInStreamLoader(ResourceLoader& loader)
{
    ASSERT(m_plugInStreamLoaders.contains(&loader));

    m_plugInStreamLoaders.remove(&loader);
    checkLoadComplete();
}

bool DocumentLoader::isMultipartReplacingLoad() const
{
    return isLoadingMultipartContent() && protectedFrameLoader()->isMultipartReplacing();
}

bool DocumentLoader::maybeLoadEmpty()
{
    bool shouldLoadEmpty = !m_substituteData.isValid() && (m_request.url().isEmpty() || LegacySchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(m_request.url().protocol()));
    Ref frameLoaderClient = frameLoader()->client();
    if (!shouldLoadEmpty && !frameLoaderClient->representationExistsForURLScheme(m_request.url().protocol()))
        return false;

    if (m_request.url().protocolIsAbout() && isHandledByAboutSchemeHandler())
        return false;

    if (m_request.url().isEmpty() && !protectedFrameLoader()->stateMachine().creatingInitialEmptyDocument()) {
        m_request.setURL(URL { aboutBlankURL() });
        if (isLoadingMainResource())
            frameLoaderClient->dispatchDidChangeProvisionalURL();
    }

    String mimeType = shouldLoadEmpty ? textHTMLContentTypeAtom() : frameLoaderClient->generatedMIMETypeForURLScheme(m_request.url().protocol());
    m_response = ResourceResponse(URL { m_request.url() }, WTFMove(mimeType), 0, "UTF-8"_s);

    bool isDisplayingInitialEmptyDocument = frameLoader()->stateMachine().isDisplayingInitialEmptyDocument();
    if (!isDisplayingInitialEmptyDocument) {
        if (auto coopEnforcementResult = doCrossOriginOpenerHandlingOfResponse(m_response)) {
            m_responseCOOP = coopEnforcementResult->crossOriginOpenerPolicy;
            if (coopEnforcementResult->needsBrowsingContextGroupSwitch)
                protectedFrameLoader()->switchBrowsingContextsGroup();
        }
    }

    SetForScope isInFinishedLoadingOfEmptyDocument { m_isInFinishedLoadingOfEmptyDocument, true };
    m_isInitialAboutBlank = isDisplayingInitialEmptyDocument;
    finishedLoading();
    return true;
}

void DocumentLoader::loadErrorDocument()
{
    m_response = ResourceResponse(URL { m_request.url() }, String { textHTMLContentTypeAtom() }, 0, "UTF-8"_s);
    SetForScope isInFinishedLoadingOfEmptyDocument { m_isInFinishedLoadingOfEmptyDocument, true };

    commitIfReady();
    if (!frameLoader())
        return;

    commitData(SharedBuffer::create());
    m_frame->document()->enforceSandboxFlags(SandboxFlag::Origin);
    m_writer.end();
}

static bool canUseServiceWorkers(LocalFrame* frame)
{
    return frame && frame->settings().serviceWorkersEnabled() && !is<HTMLPlugInElement>(frame->ownerElement());
}

bool DocumentLoader::shouldCancelLoadingAboutURL(const URL& url) const
{
    if (!url.protocolIsAbout())
        return false;

    if (url.isAboutBlank() || url.isAboutSrcDoc() || isHandledByAboutSchemeHandler())
        return false;

    if (!url.hasOpaquePath())
        return false;

#if PLATFORM(COCOA)
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::OnlyLoadWellKnownAboutURLs))
        return false;
#endif

    return true;
}

void DocumentLoader::startLoadingMainResource()
{
    RefPtr frame = m_frame.get();
    m_canUseServiceWorkers = canUseServiceWorkers(frame.get());
    m_mainDocumentError = ResourceError();
    timing().markStartTime();
    ASSERT(!m_mainResource);
    ASSERT(!m_loadingMainResource);
    m_loadingMainResource = true;

    Ref<DocumentLoader> protectedThis(*this);

    if (shouldCancelLoadingAboutURL(m_request.url())) {
        cancelMainResourceLoad(platformStrategies()->loaderStrategy()->cannotShowURLError(m_request));
        return;
    }

    if (maybeLoadEmpty()) {
        DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(DOCUMENTLOADER_STARTLOADINGMAINRESOURCE_EMTPY_DOCUMENT);
        return;
    }

#if ENABLE(CONTENT_FILTERING)
    // Always filter in WK1
    contentFilterInDocumentLoader() = frame && frame->view() && frame->protectedView()->platformWidget();
    if (contentFilterInDocumentLoader())
        m_contentFilter = !m_substituteData.isValid() ? ContentFilter::create(*this) : nullptr;
#endif

    auto url = m_request.url();
    auto fragmentDirective = url.consumeFragmentDirective();

    m_request.setURL(WTFMove(url), m_request.didFilterLinkDecoration());
    frame = m_frame.get();
    if (frame) {
        RefPtr page = frame->page();
        if (page)
            page->setMainFrameURLFragment(WTFMove(fragmentDirective));
    }

    // Make sure we re-apply the user agent to the Document's ResourceRequest upon reload in case the embedding
    // application has changed it, by clearing the previous user agent value here and applying the new value in CachedResourceLoader.
    m_request.clearHTTPUserAgent();

    ASSERT(timing().startTime());

    willSendRequest(ResourceRequest(m_request), ResourceResponse(), [this, protectedThis = Ref { *this }] (ResourceRequest&& request) mutable {
        request.setRequester(ResourceRequestRequester::Main);

        m_request = request;
        // FIXME: Implement local URL interception by getting the service worker of the parent.

        // willSendRequest() may lead to our Frame being detached or cancelling the load via nulling the ResourceRequest.
        if (!m_frame || m_request.isNull()) {
            DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource: Load canceled after willSendRequest");
            return;
        }

        // If this is a reload the cache layer might have made the previous request conditional. DocumentLoader can't handle 304 responses itself.
        request.makeUnconditional();

        DOCUMENTLOADER_RELEASE_LOG_FORWARDABLE(DOCUMENTLOADER_STARTLOADINGMAINRESOURCE_STARTING_LOAD);

        if (m_substituteData.isValid()) {
            auto url = request.url();
            matchRegistration(url, [request = WTFMove(request), protectedThis = Ref { *this }, this] (auto&& registrationData) mutable {
                if (!m_mainDocumentError.isNull()) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because of main document error (type=%d, code=%d)", static_cast<int>(m_mainDocumentError.type()), m_mainDocumentError.errorCode());
                    return;
                }
                if (!m_frame) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because no frame");
                    return;
                }

                if (registrationData)
                    m_serviceWorkerRegistrationData = makeUnique<ServiceWorkerRegistrationData>(WTFMove(*registrationData));
                // Prefer existing substitute data (from WKWebView.loadData etc) over service worker fetch.
                if (this->tryLoadingSubstituteData()) {
                    DOCUMENTLOADER_RELEASE_LOG("startLoadingMainResource callback: Load canceled because of substitute data");
                    return;
                }

                this->loadMainResource(WTFMove(request));
            });
            return;
        }
        loadMainResource(WTFMove(request));
    });
}

void DocumentLoader::unregisterReservedServiceWorkerClient()
{
    if (!m_resultingClientId)
        return;

    if (RefPtr serviceWorkerConnection = ServiceWorkerProvider::singleton().existingServiceWorkerConnection())
        serviceWorkerConnection->unregisterServiceWorkerClient(*m_resultingClientId);
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

    auto isSandboxingAllowingServiceWorkerFetchHandling = [](SandboxFlags flags) {
        return !(flags.contains(SandboxFlag::Origin)) && !(flags.contains(SandboxFlag::Scripts));
    };

    RefPtr frame = m_frame.get();
    if (!m_canUseServiceWorkers || !isSandboxingAllowingServiceWorkerFetchHandling(frame->effectiveSandboxFlags()))
        mainResourceLoadOptions.serviceWorkersMode = ServiceWorkersMode::None;
    else {
        // The main navigation load will trigger the registration of the client.
        if (m_resultingClientId) {
            scriptExecutionContextIdentifierToLoaderMap().remove(*m_resultingClientId);
            unregisterReservedServiceWorkerClient();
        }

        m_resultingClientId = ScriptExecutionContextIdentifier::generate();
        ASSERT(!scriptExecutionContextIdentifierToLoaderMap().contains(*m_resultingClientId));
        scriptExecutionContextIdentifierToLoaderMap().add(*m_resultingClientId, *this);
        mainResourceLoadOptions.resultingClientIdentifier = m_resultingClientId->object();
    }

    CachedResourceRequest mainResourceRequest(WTFMove(request), mainResourceLoadOptions);
    if (!frame->isMainFrame() && frame->document()) {
        // If we are loading the main resource of a subframe, use the cache partition of the main document.
        mainResourceRequest.setDomainForCachePartition(*frame->protectedDocument());
    } else {
        if (protectedFrameLoader()->frame().settings().storageBlockingPolicy() != StorageBlockingPolicy::BlockThirdParty)
            mainResourceRequest.setDomainForCachePartition(emptyString());
        else {
            auto origin = SecurityOrigin::create(mainResourceRequest.resourceRequest().url());
            mainResourceRequest.setDomainForCachePartition(origin->domainForCachePartition());
        }
    }

    auto mainResourceOrError = m_cachedResourceLoader->requestMainResource(WTFMove(mainResourceRequest));

    if (!mainResourceOrError) {
        // The frame may have gone away if this load was cancelled synchronously and this was the last pending load.
        // This is because we may have fired the load event in a parent frame.
        frame = m_frame.get();
        if (!frame) {
            DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, frame has gone away");
            return;
        }

        if (!m_request.url().isValid()) {
            DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, URL is invalid");
            cancelMainResourceLoad(platformStrategies()->loaderStrategy()->cannotShowURLError(m_request));
            return;
        }

        if (advancedPrivacyProtections().contains(AdvancedPrivacyProtections::HTTPSOnly)) {
            if (auto httpNavigationWithHTTPSOnlyError = platformStrategies()->loaderStrategy()->httpNavigationWithHTTPSOnlyError(m_request); mainResourceOrError.error().domain() == httpNavigationWithHTTPSOnlyError.domain()
                && mainResourceOrError.error().errorCode() == httpNavigationWithHTTPSOnlyError.errorCode()) {
                DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, URL has HTTP scheme with HTTPSOnly enabled");
                cancelMainResourceLoad(mainResourceOrError.error());
                return;
            }
        }

        DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Unable to load main resource, returning empty document");

        setRequest(ResourceRequest());
        maybeLoadEmpty();
        return;
    }

    m_mainResource = mainResourceOrError.value();

    ASSERT(frame);

#if ENABLE(CONTENT_EXTENSIONS)
    if (m_mainResource->errorOccurred() && frame->page() && m_mainResource->resourceError().domain() == ContentExtensions::WebKitContentBlockerDomain) {
        DOCUMENTLOADER_RELEASE_LOG("loadMainResource: Blocked by content blocker error");
        cancelMainResourceLoad(protectedFrameLoader()->blockedByContentBlockerError(m_request));
        return;
    }
#endif

    if (!mainResourceLoader()) {
        m_identifierForLoadWithoutResourceLoader = ResourceLoaderIdentifier::generate();
        protectedFrameLoader()->notifier().assignIdentifierToInitialRequest(*m_identifierForLoadWithoutResourceLoader, this, mainResourceRequest.resourceRequest());
        protectedFrameLoader()->notifier().dispatchWillSendRequest(this, *m_identifierForLoadWithoutResourceLoader, mainResourceRequest.resourceRequest(), ResourceResponse(), nullptr);
    }

    becomeMainResourceClient();

    // A bunch of headers are set when the underlying ResourceLoader is created, and m_request needs to include those.
    ResourceRequest updatedRequest = mainResourceLoader() ? mainResourceLoader()->originalRequest() : mainResourceRequest.resourceRequest();
    // If there was a fragment identifier on m_request, the cache will have stripped it. m_request should include
    // the fragment identifier, so add that back in.
    // Otherwise, if the main resource was loaded from a prefetch, we need to conserve the redirect URL here
    if (equalIgnoringFragmentIdentifier(m_request.url(), updatedRequest.url()) || (m_mainResource && m_mainResource->options().cachingPolicy == CachingPolicy::AllowCachingMainResourcePrefetch))
        updatedRequest.setURL(URL { m_request.url() });
    setRequest(WTFMove(updatedRequest));
}

void DocumentLoader::cancelPolicyCheckIfNeeded()
{
    if (m_waitingForContentPolicy || m_waitingForNavigationPolicy) {
        RELEASE_ASSERT(frameLoader());
        protectedFrameLoader()->policyChecker().stopCheck();
        m_waitingForContentPolicy = false;
        m_waitingForNavigationPolicy = false;
    }
}

void DocumentLoader::cancelMainResourceLoad(const ResourceError& resourceError, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    Ref<DocumentLoader> protectedThis(*this);
    ResourceError error = resourceError.isNull() ? protectedFrameLoader()->cancelledError(m_request) : resourceError;

    DOCUMENTLOADER_RELEASE_LOG("cancelMainResourceLoad: (type=%d, code=%d)", static_cast<int>(error.type()), error.errorCode());

    cancelPolicyCheckIfNeeded();

    // Don't cancel loaders for prefetch resources, as they need to survive navigation.
    if (m_mainResource && m_mainResource->options().cachingPolicy == CachingPolicy::AllowCachingMainResourcePrefetch)
        return;

    if (RefPtr loader = mainResourceLoader())
        loader->cancel(error, loadWillContinueInAnotherProcess);

    clearMainResource();

    mainReceivedError(error);
}

void DocumentLoader::willContinueMainResourceLoadAfterRedirect(const ResourceRequest& newRequest)
{
    setRequest(ResourceRequest { newRequest });
}

void DocumentLoader::clearMainResource()
{
    ASSERT(isMainThread());
    if (m_mainResource && m_mainResource->hasClient(*this))
        m_mainResource->removeClient(*this);
#if ENABLE(CONTENT_FILTERING)
    if (m_contentFilter)
        m_contentFilter->stopFilteringMainResource();
#endif

    m_mainResource = nullptr;
    m_isContinuingLoadAfterProvisionalLoadStarted = false;

    unregisterReservedServiceWorkerClient();
}

void DocumentLoader::subresourceLoaderFinishedLoadingOnePart(ResourceLoader& loader)
{
    if (!m_multipartSubresourceLoaders.add(&loader).isNewEntry)
        ASSERT(!m_subresourceLoaders.contains(&loader));
    else {
        ASSERT(m_subresourceLoaders.contains(&loader));
        m_subresourceLoaders.remove(&loader);
    }

    checkLoadComplete();
    if (m_frame)
        m_frame->loader().checkLoadComplete();
}

void DocumentLoader::maybeFinishLoadingMultipartContent()
{
    if (!isMultipartReplacingLoad())
        return;

    protectedFrameLoader()->setupForMultipartReplace();
    m_committed = false;
    commitLoad(mainResourceData()->makeContiguous());
}

void DocumentLoader::startIconLoading()
{
    static uint64_t nextIconCallbackID = 1;

    RefPtr document = this->document();
    if (!document)
        return;

    if (!m_frame->isMainFrame())
        return;

    if (document->url().isEmpty() || document->url().protocolIsAbout())
        return;

    m_linkIcons = LinkIconCollector { *document }.iconsOfTypes({ LinkIconType::Favicon, LinkIconType::TouchIcon, LinkIconType::TouchPrecomposedIcon });

    auto findResult = m_linkIcons.findIf([](auto& icon) { return icon.type == LinkIconType::Favicon; });
    if (findResult == notFound && document->url().protocolIsInHTTPFamily())
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

    Ref iconLoader = IconLoader::create(*this, icon.url);
    m_iconLoaders.add(iconLoader.copyRef(), WTFMove(completionHandler));

    iconLoader->startLoading();
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

    if (RefPtr document = this->document(); document && document->isSameOriginAsTopDocument())
        return m_shouldOpenExternalURLsPolicy;

    return ShouldOpenExternalURLsPolicy::ShouldNotAllow;
}

// https://www.w3.org/TR/css-view-transitions-2/#navigation-can-trigger-a-cross-document-view-transition
bool DocumentLoader::navigationCanTriggerCrossDocumentViewTransition(Document& oldDocument, bool fromBackForwardCache)
{
    if (loadStartedDuringSwipeAnimation())
        return false;

    if (std::holds_alternative<Document::SkipTransition>(oldDocument.resolveViewTransitionRule()))
        return false;

    if (!m_triggeringAction.navigationAPIType() || *m_triggeringAction.navigationAPIType() == NavigationNavigationType::Reload)
        return false;

    Ref newOrigin = SecurityOrigin::create(documentURL());
    if (!newOrigin->isSameOriginAs(oldDocument.protectedSecurityOrigin()))
        return false;

    if (const auto* metrics = response().deprecatedNetworkLoadMetricsOrNull(); metrics && !fromBackForwardCache) {
        if (metrics->crossOriginRedirect())
            return false;
    }

    if (*m_triggeringAction.navigationAPIType() == NavigationNavigationType::Traverse)
        return true;

    if (isRequestFromClientOrUserInput())
        return false;

    return true;
}

void DocumentLoader::becomeMainResourceClient()
{
#if ENABLE(CONTENT_FILTERING)
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

void DocumentLoader::previewResponseReceived(const CachedResource& resource, const ResourceResponse& response)
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
    protectedFrame()->protectedDocument()->addConsoleMessage(messageSource, messageLevel, message, requestIdentifier);
}

void DocumentLoader::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&& eventInit)
{
    protectedFrame()->protectedDocument()->enqueueSecurityPolicyViolationEvent(WTFMove(eventInit));
}

#if ENABLE(CONTENT_FILTERING)
void DocumentLoader::dataReceivedThroughContentFilter(const SharedBuffer& buffer)
{
    dataReceived(buffer);
}

void DocumentLoader::cancelMainResourceLoadForContentFilter(const ResourceError& error)
{
    cancelMainResourceLoad(error);
}

ResourceError DocumentLoader::contentFilterDidBlock(ContentFilterUnblockHandler&& unblockHandler, String&& unblockRequestDeniedScript)
{
    return handleContentFilterDidBlock(WTFMove(unblockHandler), WTFMove(unblockRequestDeniedScript));
}

void DocumentLoader::handleProvisionalLoadFailureFromContentFilter(const URL& blockedPageURL, SubstituteData&& substituteData)
{
    protectedFrameLoader()->load(FrameLoadRequest(*frame(), URL { blockedPageURL }, WTFMove(substituteData)));
}

#if HAVE(WEBCONTENTRESTRICTIONS_PATH_SPI)
String DocumentLoader::webContentRestrictionsConfigurationPath() const
{
    return emptyString();
}
#endif
#endif // ENABLE(CONTENT_FILTERING)

#if ENABLE(CONTENT_FILTERING)
ResourceError DocumentLoader::handleContentFilterDidBlock(ContentFilterUnblockHandler&& unblockHandler, String&& unblockRequestDeniedScript)
{
    unblockHandler.setUnreachableURL(documentURL());
    if (!unblockRequestDeniedScript.isEmpty() && frame()) {
        unblockHandler.wrapWithDecisionHandler([scriptController = WeakPtr { frame()->script() }, script = WTFMove(unblockRequestDeniedScript).isolatedCopy()](bool unblocked) {
            if (!unblocked && scriptController) {
                // FIXME: This probably needs to figure out if the origin is considered tainted.
                scriptController->executeScriptIgnoringException(script, JSC::SourceTaintedOrigin::Untainted);
            }
        });
    }
    protectedFrameLoader()->client().contentFilterDidBlockLoad(WTFMove(unblockHandler));
    auto error = protectedFrameLoader()->blockedByContentFilterError(request());

    m_blockedByContentFilter = true;
    m_blockedError = error;

    return error;
}

bool DocumentLoader::contentFilterWillHandleProvisionalLoadFailure(const ResourceError& error)
{
    if (m_contentFilter && m_contentFilter->willHandleProvisionalLoadFailure(error))
        return true;
    if (contentFilterInDocumentLoader())
        return false;
    return m_blockedByContentFilter && m_blockedError.errorCode() == error.errorCode() && m_blockedError.domain() == error.domain();
}

void DocumentLoader::contentFilterHandleProvisionalLoadFailure(const ResourceError& error)
{
    if (m_contentFilter)
        m_contentFilter->handleProvisionalLoadFailure(error);
    if (contentFilterInDocumentLoader())
        return;
    handleProvisionalLoadFailureFromContentFilter(m_blockedPageURL, SubstituteData { m_substituteDataFromContentFilter });
}

#endif // ENABLE(CONTENT_FILTERING)

void DocumentLoader::setActiveContentRuleListActionPatterns(const HashMap<String, Vector<String>>& patterns)
{
    MemoryCompactRobinHoodHashMap<String, Vector<UserContentURLPattern>> parsedPatternMap;

    for (auto& pair : patterns) {
        auto patternVector = WTF::compactMap(pair.value, [](auto& patternString) -> std::optional<UserContentURLPattern> {
            UserContentURLPattern parsedPattern(patternString);
            if (parsedPattern.isValid())
                return parsedPattern;
            return std::nullopt;
        });
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

void DocumentLoader::setHTTPSByDefaultMode(HTTPSByDefaultMode mode)
{
    if (mode == HTTPSByDefaultMode::Disabled) {
        if (m_advancedPrivacyProtections.contains(AdvancedPrivacyProtections::HTTPSOnly))
            m_httpsByDefaultMode = HTTPSByDefaultMode::UpgradeWithUserMediatedFallback;
        else if (m_advancedPrivacyProtections.contains(AdvancedPrivacyProtections::HTTPSFirst))
            m_httpsByDefaultMode = HTTPSByDefaultMode::UpgradeWithAutomaticFallback;
    } else
        m_httpsByDefaultMode = mode;
}

void DocumentLoader::setPreferences(WebpagePreferences&& preferences)
{
    m_preferences = WTFMove(preferences);
}

Ref<CachedResourceLoader> DocumentLoader::protectedCachedResourceLoader() const
{
    return m_cachedResourceLoader;
}

void DocumentLoader::whenDocumentIsCreated(Function<void(Document*)>&& callback)
{
    ASSERT(!m_canUseServiceWorkers || !!m_resultingClientId);

    if (auto previousCallback = std::exchange(m_whenDocumentIsCreatedCallback, { })) {
        callback = [previousCallback = WTFMove(previousCallback), newCallback = WTFMove(callback)] (auto* document) mutable {
            previousCallback(document);
            newCallback(document);
        };
    }
    m_whenDocumentIsCreatedCallback = WTFMove(callback);
}

void DocumentLoader::setNewResultingClientId(ScriptExecutionContextIdentifier identifier)
{
    if (scriptExecutionContextIdentifierToLoaderMap().remove(*m_resultingClientId)) {
        m_resultingClientId = identifier;
        scriptExecutionContextIdentifierToLoaderMap().add(identifier, this);
    }
}

std::unique_ptr<IntegrityPolicy> DocumentLoader::integrityPolicy()
{
    return WTFMove(m_integrityPolicy);
}

std::unique_ptr<IntegrityPolicy> DocumentLoader::integrityPolicyReportOnly()
{
    return WTFMove(m_integrityPolicyReportOnly);
}

} // namespace WebCore

#undef PAGE_ID
#undef FRAME_ID
#undef IS_MAIN_FRAME
