/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebLoaderStrategy.h"

#include "HangDetectionDisabler.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "SessionTracker.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebFrameNetworkingContext.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebResourceLoader.h"
#include <WebCore/ApplicationCacheHost.h>
#include <WebCore/CachedResource.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/SessionID.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <wtf/text/CString.h>

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(...) RELEASE_LOG_IF(loadParameters.sessionID.isAlwaysOnLoggingAllowed(), __VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(...) RELEASE_LOG_ERROR_IF(loadParameters.sessionID.isAlwaysOnLoggingAllowed(), __VA_ARGS__)

namespace WebKit {

WebLoaderStrategy::WebLoaderStrategy()
    : m_internallyFailedLoadTimer(RunLoop::main(), this, &WebLoaderStrategy::internallyFailedLoadTimerFired)
{
}

WebLoaderStrategy::~WebLoaderStrategy()
{
}

RefPtr<SubresourceLoader> WebLoaderStrategy::loadResource(Frame& frame, CachedResource& resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    RefPtr<SubresourceLoader> loader = SubresourceLoader::create(frame, resource, request, options);
    if (loader)
        scheduleLoad(*loader, &resource, frame.document()->referrerPolicy() == ReferrerPolicy::Default);
    return loader;
}

RefPtr<NetscapePlugInStreamLoader> WebLoaderStrategy::schedulePluginStreamLoad(Frame& frame, NetscapePlugInStreamLoaderClient& client, const ResourceRequest& request)
{
    RefPtr<NetscapePlugInStreamLoader> loader = NetscapePlugInStreamLoader::create(frame, client, request);
    if (loader)
        scheduleLoad(*loader, 0, frame.document()->referrerPolicy() == ReferrerPolicy::Default);
    return loader;
}

static std::chrono::milliseconds maximumBufferingTime(CachedResource* resource)
{
#if !ENABLE(NETWORK_CACHE)
    return 0ms;
#endif

    if (!resource)
        return 0ms;

    switch (resource->type()) {
    case CachedResource::CSSStyleSheet:
    case CachedResource::Script:
#if ENABLE(SVG_FONTS)
    case CachedResource::SVGFontResource:
#endif
    case CachedResource::FontResource:
        return std::chrono::milliseconds::max();
    case CachedResource::ImageResource:
        return 500ms;
    case CachedResource::MediaResource:
    case CachedResource::MainResource:
    case CachedResource::RawResource:
    case CachedResource::SVGDocumentResource:
    case CachedResource::LinkPreload:
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
    case CachedResource::LinkSubresource:
#endif
#if ENABLE(VIDEO_TRACK)
    case CachedResource::TextTrackResource:
#endif
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return 0ms;
    }

    ASSERT_NOT_REACHED();
    return 0ms;
}

void WebLoaderStrategy::scheduleLoad(ResourceLoader& resourceLoader, CachedResource* resource, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    ResourceLoadIdentifier identifier = resourceLoader.identifier();
    ASSERT(identifier);

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    // If the DocumentLoader schedules this as an archive resource load,
    // then we should remember the ResourceLoader in our records but not schedule it in the NetworkProcess.
    if (resourceLoader.documentLoader()->scheduleArchiveLoad(resourceLoader, resourceLoader.request())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as an archive resource.", resourceLoader.url().string().utf8().data());
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader));
        return;
    }
#endif

    if (resourceLoader.documentLoader()->applicationCacheHost()->maybeLoadResource(resourceLoader, resourceLoader.request(), resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded from application cache.", resourceLoader.url().string().utf8().data());
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader));
        return;
    }

    if (resourceLoader.request().url().protocolIsData()) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded as data.", resourceLoader.url().string().utf8().data());
        startLocalLoad(resourceLoader);
        return;
    }

#if USE(QUICK_LOOK)
    if (resourceLoader.request().url().protocolIs(QLPreviewProtocol())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a QuickLook resource.", resourceLoader.url().string().utf8().data());
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if USE(SOUP)
    // For apps that call g_resource_load in a web extension.
    // https://blogs.gnome.org/alexl/2012/01/26/resources-in-glib/
    if (resourceLoader.request().url().protocolIs("resource")) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a GResource.", resourceLoader.url().string().utf8().data());
        startLocalLoad(resourceLoader);
        return;
    }
#endif

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be scheduled with the NetworkProcess with priority %d", resourceLoader.url().string().latin1().data(), static_cast<int>(resourceLoader.request().priority()));

    ContentSniffingPolicy contentSniffingPolicy = resourceLoader.shouldSniffContent() ? SniffContent : DoNotSniffContent;
    StoredCredentials allowStoredCredentials = resourceLoader.shouldUseCredentialStorage() ? AllowStoredCredentials : DoNotAllowStoredCredentials;

    // FIXME: Some entities in WebCore use WebCore's "EmptyFrameLoaderClient" instead of having a proper WebFrameLoaderClient.
    // EmptyFrameLoaderClient shouldn't exist and everything should be using a WebFrameLoaderClient,
    // but in the meantime we have to make sure not to mis-cast.
    WebFrameLoaderClient* webFrameLoaderClient = toWebFrameLoaderClient(resourceLoader.frameLoader()->client());
    WebFrame* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : 0;
    WebPage* webPage = webFrame ? webFrame->page() : 0;

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = identifier;
    loadParameters.webPageID = webPage ? webPage->pageID() : 0;
    loadParameters.webFrameID = webFrame ? webFrame->frameID() : 0;
    loadParameters.sessionID = webPage ? webPage->sessionID() : SessionID::defaultSessionID();
    loadParameters.request = resourceLoader.request();
    loadParameters.contentSniffingPolicy = contentSniffingPolicy;
    loadParameters.allowStoredCredentials = allowStoredCredentials;
    // If there is no WebFrame then this resource cannot be authenticated with the client.
    loadParameters.clientCredentialPolicy = (webFrame && webPage && resourceLoader.isAllowedToAskUserForCredentials()) ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect;
    loadParameters.defersLoading = resourceLoader.defersLoading();
    loadParameters.needsCertificateInfo = resourceLoader.shouldIncludeCertificateInfo();
    loadParameters.maximumBufferingTime = maximumBufferingTime(resource);

    ASSERT((loadParameters.webPageID && loadParameters.webFrameID) || loadParameters.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials);

    if (!WebProcess::singleton().networkConnection().connection().send(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(loadParameters), 0)) {
        RELEASE_LOG_ERROR_IF_ALLOWED("WebLoaderStrategy::scheduleLoad: Unable to schedule resource with the NetworkProcess with priority = %d, pageID = %llu, frameID = %llu", static_cast<int>(resourceLoader.request().priority()), static_cast<unsigned long long>(loadParameters.webPageID), static_cast<unsigned long long>(loadParameters.webFrameID));
        // We probably failed to schedule this load with the NetworkProcess because it had crashed.
        // This load will never succeed so we will schedule it to fail asynchronously.
        scheduleInternallyFailedLoad(resourceLoader);
        return;
    }

    auto webResourceLoader = WebResourceLoader::create(resourceLoader);
    RELEASE_LOG_IF_ALLOWED("WebLoaderStrategy::scheduleLoad: Resource will be scheduled with the NetworkProcess with priority = %d, pageID = %llu, frameID = %llu, WebResourceLoader = %p", static_cast<int>(resourceLoader.request().priority()), static_cast<unsigned long long>(loadParameters.webPageID), static_cast<unsigned long long>(loadParameters.webFrameID), webResourceLoader.ptr());
    m_webResourceLoaders.set(identifier, WTFMove(webResourceLoader));
}

void WebLoaderStrategy::scheduleInternallyFailedLoad(WebCore::ResourceLoader& resourceLoader)
{
    m_internallyFailedResourceLoaders.add(&resourceLoader);
    m_internallyFailedLoadTimer.startOneShot(0);
}

void WebLoaderStrategy::internallyFailedLoadTimerFired()
{
    Vector<RefPtr<ResourceLoader>> internallyFailedResourceLoaders;
    copyToVector(m_internallyFailedResourceLoaders, internallyFailedResourceLoaders);
    
    for (size_t i = 0; i < internallyFailedResourceLoaders.size(); ++i)
        internallyFailedResourceLoaders[i]->didFail(internalError(internallyFailedResourceLoaders[i]->url()));
}

void WebLoaderStrategy::startLocalLoad(WebCore::ResourceLoader& resourceLoader)
{
    resourceLoader.start();
    m_webResourceLoaders.set(resourceLoader.identifier(), WebResourceLoader::create(resourceLoader));
}

void WebLoaderStrategy::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);
    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::remove, url '%s'", resourceLoader->url().string().utf8().data());

    if (m_internallyFailedResourceLoaders.contains(resourceLoader)) {
        m_internallyFailedResourceLoaders.remove(resourceLoader);
        return;
    }
    
    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebLoaderStrategy removing a ResourceLoader that has no identifier.");
        return;
    }
    
    RefPtr<WebResourceLoader> loader = m_webResourceLoaders.take(identifier);
    // Loader may not be registered if we created it, but haven't scheduled yet (a bundle client can decide to cancel such request via willSendRequest).
    if (!loader)
        return;

    WebProcess::singleton().networkConnection().connection().send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(identifier), 0);

    // It's possible that this WebResourceLoader might be just about to message back to the NetworkProcess (e.g. ContinueWillSendRequest)
    // but there's no point in doing so anymore.
    loader->detachFromCoreLoader();
}

void WebLoaderStrategy::setDefersLoading(ResourceLoader* resourceLoader, bool defers)
{
    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    WebProcess::singleton().networkConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetDefersLoading(identifier, defers), 0);
}

void WebLoaderStrategy::crossOriginRedirectReceived(ResourceLoader*, const URL&)
{
    // We handle cross origin redirects entirely within the NetworkProcess.
    // We override this call in the WebProcess to make it a no-op.
}

void WebLoaderStrategy::servePendingRequests(ResourceLoadPriority)
{
    // This overrides the base class version.
    // We don't need to do anything as this is handled by the network process.
}

void WebLoaderStrategy::suspendPendingRequests()
{
    // Network process does keep requests in pending state.
}

void WebLoaderStrategy::resumePendingRequests()
{
    // Network process does keep requests in pending state.
}

void WebLoaderStrategy::networkProcessCrashed()
{
    for (auto& loader : m_webResourceLoaders)
        scheduleInternallyFailedLoad(*loader.value->resourceLoader());

    m_webResourceLoaders.clear();
}

void WebLoaderStrategy::loadResourceSynchronously(NetworkingContext* context, unsigned long resourceLoadIdentifier, const ResourceRequest& request, StoredCredentials storedCredentials, ClientCredentialPolicy clientCredentialPolicy, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    WebFrameNetworkingContext* webContext = static_cast<WebFrameNetworkingContext*>(context);
    // FIXME: Some entities in WebCore use WebCore's "EmptyFrameLoaderClient" instead of having a proper WebFrameLoaderClient.
    // EmptyFrameLoaderClient shouldn't exist and everything should be using a WebFrameLoaderClient,
    // but in the meantime we have to make sure not to mis-cast.
    WebFrameLoaderClient* webFrameLoaderClient = webContext->webFrameLoaderClient();
    WebFrame* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : 0;
    WebPage* webPage = webFrame ? webFrame->page() : 0;

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = resourceLoadIdentifier;
    loadParameters.webPageID = webPage ? webPage->pageID() : 0;
    loadParameters.webFrameID = webFrame ? webFrame->frameID() : 0;
    loadParameters.sessionID = webPage ? webPage->sessionID() : SessionID::defaultSessionID();
    loadParameters.request = request;
    loadParameters.contentSniffingPolicy = SniffContent;
    loadParameters.allowStoredCredentials = storedCredentials;
    loadParameters.clientCredentialPolicy = clientCredentialPolicy;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = context->shouldClearReferrerOnHTTPSToHTTPRedirect();

    data.resize(0);

    HangDetectionDisabler hangDetectionDisabler;

    if (!WebProcess::singleton().networkConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad(loadParameters), Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad::Reply(error, response, data), 0)) {
        response = ResourceResponse();
        error = internalError(request.url());
    }
}

void WebLoaderStrategy::createPingHandle(NetworkingContext* networkingContext, ResourceRequest& request, bool shouldUseCredentialStorage)
{
    // It's possible that call to createPingHandle might be made during initial empty Document creation before a NetworkingContext exists.
    // It is not clear that we should send ping loads during that process anyways.
    if (!networkingContext)
        return;

    WebFrameNetworkingContext* webContext = static_cast<WebFrameNetworkingContext*>(networkingContext);
    WebFrameLoaderClient* webFrameLoaderClient = webContext->webFrameLoaderClient();
    WebFrame* webFrame = webFrameLoaderClient ? webFrameLoaderClient->webFrame() : nullptr;
    WebPage* webPage = webFrame ? webFrame->page() : nullptr;
    
    NetworkResourceLoadParameters loadParameters;
    loadParameters.request = request;
    loadParameters.sessionID = webPage ? webPage->sessionID() : SessionID::defaultSessionID();
    loadParameters.allowStoredCredentials = shouldUseCredentialStorage ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = networkingContext->shouldClearReferrerOnHTTPSToHTTPRedirect();

    WebProcess::singleton().networkConnection().connection().send(Messages::NetworkConnectionToWebProcess::LoadPing(loadParameters), 0);
}


} // namespace WebKit
