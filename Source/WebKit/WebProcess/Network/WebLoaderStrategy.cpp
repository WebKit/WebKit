/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#include "RemoteWorkerFrameLoaderClient.h"
#include "WebCompiledContentRuleList.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebPage.h"
#include "WebPageInlines.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebResourceLoader.h"
#include "WebSWContextManagerConnection.h"
#include "WebServiceWorkerProvider.h"
#include "WebURLSchemeHandlerProxy.h"
#include "WebURLSchemeTaskProxy.h"
#include <WebCore/CachedResource.h>
#include <WebCore/ContentSecurityPolicy.h>
#include <WebCore/DataURLDecoder.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/InspectorInstrumentationWebKit.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkLoadInformation.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/UserContentProvider.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>

#if USE(QUICK_LOOK)
#include <WebCore/QuickLook.h>
#endif

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
#include "WebExtensionControllerProxy.h"
#endif

#define WEBLOADERSTRATEGY_RELEASE_LOG_BASIC(fmt, ...) RELEASE_LOG(Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC(fmt, ...) RELEASE_LOG_ERROR(Network, "%p - WebLoaderStrategy::" fmt, this, ##__VA_ARGS__)

#define WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE "%p - [resourceLoader=%p, frameLoader=%p, frame=%p, webPageID=%" PRIu64 ", frameID=%" PRIu64 ", resourceID=%" PRIu64 "] WebLoaderStrategy::"
#define WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS this, &resourceLoader, resourceLoader.frameLoader(), resourceLoader.frame(), pageIDForLog(trackingParameters), frameIDForLog(trackingParameters), resourceIDForLog(trackingParameters)
#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS this, nullptr, &frameLoader, &frameLoader.frame(), pageIDForLog(trackingParameters), frameIDForLog(trackingParameters), resourceIDForLog(trackingParameters)

#define WEBLOADERSTRATEGY_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_RELEASE_LOG_FORWARDABLE(fmt, ...) RELEASE_LOG_FORWARDABLE(Network, fmt, pageIDForLog(trackingParameters), frameIDForLog(trackingParameters), resourceIDForLog(trackingParameters), ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)

#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)
#define WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(Network, WEBLOADERSTRATEGY_RELEASE_LOG_STANDARD_TEMPLATE fmt, WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_STANDARD_PARAMETERS, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebLoaderStrategy);

[[maybe_unused]] static uint64_t pageIDForLog(const std::optional<WebResourceLoader::TrackingParameters>& parameters)
{
    return parameters ? parameters->pageID.toUInt64() : 0;
}

[[maybe_unused]] static uint64_t frameIDForLog(const std::optional<WebResourceLoader::TrackingParameters>& parameters)
{
    return parameters ? parameters->frameID.toUInt64() : 0;
}

[[maybe_unused]] static uint64_t resourceIDForLog(const std::optional<WebResourceLoader::TrackingParameters>& parameters)
{
    return parameters ? parameters->resourceID.toUInt64() : 0;
}

WebLoaderStrategy::WebLoaderStrategy(WebProcess& webProcess)
    : m_webProcess(webProcess)
    , m_internallyFailedLoadTimer(RunLoop::mainSingleton(), "WebLoaderStrategy::InternallyFailedLoadTimer"_s, this, &WebLoaderStrategy::internallyFailedLoadTimerFired)
{
}

WebLoaderStrategy::~WebLoaderStrategy() = default;

void WebLoaderStrategy::ref() const
{
    m_webProcess->ref();
}

void WebLoaderStrategy::deref() const
{
    m_webProcess->deref();
}

void WebLoaderStrategy::loadResource(LocalFrame& frame, CachedResource& resource, ResourceRequest&& request, const ResourceLoaderOptions& options, CompletionHandler<void(RefPtr<SubresourceLoader>&&)>&& completionHandler)
{
    if (resource.type() != CachedResource::Type::MainResource || !frame.isMainFrame()) {
        if (RefPtr localMainFrame = frame.localMainFrame()) {
            if (RefPtr document = localMainFrame->document()) {
                if (document && document->loader())
                    request.setIsAppInitiated(document->loader()->lastNavigationWasAppInitiated());
            }
        }
    }

    SubresourceLoader::create(frame, resource, WTF::move(request), options, [this, protectedThis = Ref { *this }, referrerPolicy = options.referrerPolicy, completionHandler = WTF::move(completionHandler), resource = CachedResourceHandle<CachedResource>(&resource), frame = Ref { frame }] (RefPtr<SubresourceLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(*loader, resource.get(), referrerPolicy == ReferrerPolicy::NoReferrerWhenDowngrade);
        else
            RELEASE_LOG(Network, "%p - [webPageID=%" PRIu64 ", frameID=%" PRIu64 "] WebLoaderStrategy::loadResource: Unable to create SubresourceLoader", this, frame->pageID() ? frame->pageID()->toUInt64() : 0, frame->frameID().toUInt64());
        completionHandler(WTF::move(loader));
    });
}

void WebLoaderStrategy::schedulePluginStreamLoad(LocalFrame& frame, NetscapePlugInStreamLoaderClient& client, ResourceRequest&& request, CompletionHandler<void(RefPtr<NetscapePlugInStreamLoader>&&)>&& completionHandler)
{
    NetscapePlugInStreamLoader::create(frame, client, WTF::move(request), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), frame = Ref { frame }] (RefPtr<NetscapePlugInStreamLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(*loader, 0, frame->document()->referrerPolicy() == ReferrerPolicy::NoReferrerWhenDowngrade);
        completionHandler(WTF::move(loader));
    });
}

static Seconds maximumBufferingTime(CachedResource* resource)
{
    if (!resource)
        return 0_s;

    switch (resource->type()) {
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::CSSStyleSheet:
    case CachedResource::Type::JSON:
    case CachedResource::Type::Script:
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
        return Seconds::infinity();
    case CachedResource::Type::ImageResource:
        return 500_ms;
    case CachedResource::Type::MediaResource:
        return WebLoaderStrategy::mediaMaximumBufferingTime;
    case CachedResource::Type::MainResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    case CachedResource::Type::SVGDocumentResource:
    case CachedResource::Type::LinkPrefetch:
#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
#endif
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
        return 0_s;
    }

    ASSERT_NOT_REACHED();
    return 0_s;
}

void WebLoaderStrategy::scheduleLoad(ResourceLoader& resourceLoader, CachedResource* resource, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    auto identifier = *resourceLoader.identifier();

    RefPtr frameLoader = resourceLoader.frameLoader();
    if (!frameLoader) {
        ASSERT_NOT_REACHED();
        return;
    }
    auto& frameLoaderClient = frameLoader->client();
    auto pageID = frameLoader->frame().pageID();

    std::optional<WebPageProxyIdentifier> webPageProxyID;
    if (RefPtr webFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(frameLoaderClient))
        webPageProxyID = webFrameLoaderClient->webPageProxyID();
    else if (RefPtr workerFrameLoaderClient = dynamicDowncast<RemoteWorkerFrameLoaderClient>(frameLoaderClient))
        webPageProxyID = workerFrameLoaderClient->webPageProxyID();

    auto trackingParameters = webPageProxyID && pageID ? std::optional(WebResourceLoader::TrackingParameters {
        *webPageProxyID,
        *pageID,
        frameLoader->frameID(),
        identifier
    }) : std::nullopt;

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    // If the DocumentLoader schedules this as an archive resource load,
    // then we should remember the ResourceLoader in our records but not schedule it in the NetworkProcess.
    if (protect(resourceLoader.documentLoader())->scheduleArchiveLoad(resourceLoader, resourceLoader.request())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as an archive resource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as an archive resource");
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader, trackingParameters));
        return;
    }
#endif

    if (resourceLoader.request().url().protocolIsData()) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be loaded as data.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG_FORWARDABLE(WEBLOADERSTRATEGY_SCHEDULELOAD_URL_LOADED_AS_DATA);
        startLocalLoad(resourceLoader);
        return;
    }

#if USE(QUICK_LOOK)
    if (isQuickLookPreviewURL(resourceLoader.request().url())) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a QuickLook resource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as a QuickLook resource");
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if USE(SOUP)
    // For apps that call g_resource_load in a web extension.
    // https://blogs.gnome.org/alexl/2012/01/26/resources-in-glib/
    if (resourceLoader.request().url().protocolIs("resource"_s)) {
        LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a GResource.", resourceLoader.url().string().utf8().data());
        WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: URL will be handled as a GResource");
        startLocalLoad(resourceLoader);
        return;
    }
#endif

#if ENABLE(PDFJS)
    if (tryLoadingUsingPDFJSHandler(resourceLoader, trackingParameters))
        return;
#endif

    if (tryLoadingUsingURLSchemeHandler(resourceLoader, trackingParameters))
        return;

#if ENABLE(SWIFT_DEMO_URI_SCHEME)
    if (resourceLoader.request().url().protocolIs("x-swift-demo"_s)) {
        // We don't load this at all - higher layers of webkit code will call loadData later.
        return;
    }
#endif

    if (!trackingParameters) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (InspectorInstrumentationWebKit::shouldInterceptRequest(resourceLoader)) {
        InspectorInstrumentationWebKit::interceptRequest(resourceLoader, [this, protectedThis = Ref { *this }, protectedResourceLoader = Ref { resourceLoader }, trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, resource](const ResourceRequest& request) {
            auto& resourceLoader = protectedResourceLoader.get();
            WEBLOADERSTRATEGY_RELEASE_LOG("scheduleLoad: intercepted URL will be scheduled with the NetworkProcess");
            scheduleLoadFromNetworkProcess(resourceLoader, request, *trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime(resource));
        });
        return;
    }

    WEBLOADERSTRATEGY_RELEASE_LOG_FORWARDABLE(WEBLOADERSTRATEGY_SCHEDULELOAD);
    scheduleLoadFromNetworkProcess(resourceLoader, resourceLoader.request(), *trackingParameters, shouldClearReferrerOnHTTPSToHTTPRedirect, maximumBufferingTime(resource));
}

bool WebLoaderStrategy::tryLoadingUsingURLSchemeHandler(ResourceLoader& resourceLoader, const std::optional<WebResourceLoader::TrackingParameters>& trackingParameters)
{
    RefPtr<WebPage> webPage;
    RefPtr<WebFrame> webFrame;

    if (!resourceLoader.frameLoader())
        return false;

    if (auto* webFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(resourceLoader.frameLoader()->client())) {
        webFrame = webFrameLoaderClient->webFrame();
        webPage = webFrame->page();
    } else if (auto* workerFrameLoaderClient = dynamicDowncast<RemoteWorkerFrameLoaderClient>(resourceLoader.frameLoader()->client())) {
        if (auto serviceWorkerPageIdentifier = workerFrameLoaderClient->serviceWorkerPageIdentifier()) {
            if (RefPtr page = Page::serviceWorkerPage(*serviceWorkerPageIdentifier)) {
                webPage = WebPage::fromCorePage(*page);
                RefPtr frame = webPage ? webPage->mainFrame() : nullptr;
                webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;
            }
        }
    }

    if (!webPage || !webFrame)
        return false;

    if (resourceLoader.request().url().protocolIsAbout() && !resourceLoader.documentLoader()->isLoadingMainResource())
        return false;

    RefPtr handler = webPage->urlSchemeHandlerForScheme(resourceLoader.request().url().protocol());
    if (!handler)
        return false;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, URL '%s' will be handled by a UIProcess URL scheme handler.", resourceLoader.url().string().utf8().data());
    WEBLOADERSTRATEGY_RELEASE_LOG("tryLoadingUsingURLSchemeHandler: URL will be handled by a UIProcess URL scheme handler");

    handler->startNewTask(resourceLoader, *webFrame);
    return true;
}

#if ENABLE(PDFJS)
bool WebLoaderStrategy::tryLoadingUsingPDFJSHandler(ResourceLoader& resourceLoader, const std::optional<WebResourceLoader::TrackingParameters>& trackingParameters)
{
    if (!resourceLoader.request().url().protocolIs("webkit-pdfjs-viewer"_s))
        return false;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be handled as a PDFJS resource.", resourceLoader.url().string().utf8().data());
    WEBLOADERSTRATEGY_RELEASE_LOG("tryLoadingUsingPDFJSHandler: URL will be scheduled with the PDFJS url scheme handler");

    startLocalLoad(resourceLoader);
    return true;
}
#endif

static RefPtr<DocumentLoader> policySourceDocumentLoaderForFrame(const LocalFrame& frame, bool isMainFrameNavigation = false)
{
    RefPtr mainFrame = frame.localMainFrame();
    if (!mainFrame)
        return { };

    auto canIncludeCurrentDocumentLoader = isMainFrameNavigation ? FrameLoader::CanIncludeCurrentDocumentLoader::No : FrameLoader::CanIncludeCurrentDocumentLoader::Yes;
    auto mainFrameDocumentLoader = mainFrame->loader().loaderForWebsitePolicies(canIncludeCurrentDocumentLoader);

    auto policySourceDocumentLoader = mainFrameDocumentLoader;
    if (policySourceDocumentLoader && !policySourceDocumentLoader->request().url().hasSpecialScheme() && frame.document()->url().protocolIsInHTTPFamily())
        policySourceDocumentLoader = frame.loader().documentLoader();

    return policySourceDocumentLoader;
}

static void addParametersShared(const LocalFrame* frame, NetworkResourceLoadParameters& parameters, bool isMainFrameNavigation = false)
{
    parameters.crossOriginAccessControlCheckEnabled = CrossOriginAccessControlCheckDisabler::singleton().crossOriginAccessControlCheckEnabled();
    parameters.hadMainFrameMainResourcePrivateRelayed = WebProcess::singleton().hadMainFrameMainResourcePrivateRelayed();

    if (!frame)
        return;

    // When loading the main frame, we need to get allowPrivacyProxy from the same DocumentLoader that
    // WebLocalFrameLoaderClient::applyToDocumentLoader stored the value on. Otherwise, we need to get the
    // value from the main frame's current DocumentLoader.

    Ref mainFrame = frame->mainFrame();
    RefPtr policySourceDocumentLoader = policySourceDocumentLoaderForFrame(*frame, isMainFrameNavigation);

    parameters.allowPrivacyProxy = policySourceDocumentLoader ? policySourceDocumentLoader->allowPrivacyProxy() : true;

    if (RefPtr framePolicySourceDocumentLoader = frame->loader().loaderForWebsitePolicies(isMainFrameNavigation ? FrameLoader::CanIncludeCurrentDocumentLoader::No : FrameLoader::CanIncludeCurrentDocumentLoader::Yes)) {
        if (String referrer = framePolicySourceDocumentLoader->preferences().overrideReferrerForAllRequests; !referrer.isNull())
            parameters.request.setHTTPHeaderField(HTTPHeaderName::Referer, referrer);
    }

    if (auto* document = frame->document()) {
        parameters.crossOriginEmbedderPolicy = document->crossOriginEmbedderPolicy();
        parameters.isClearSiteDataHeaderEnabled = document->settings().clearSiteDataHTTPHeaderEnabled();
        parameters.isClearSiteDataExecutionContextEnabled = document->settings().clearSiteDataExecutionContextsSupportEnabled();
    }

    if (RefPtr page = frame->page()) {
        parameters.pageHasResourceLoadClient = page->hasResourceLoadClient();
        page->logMediaDiagnosticMessage(parameters.request.httpBody());

        if (RefPtr webPage = WebPage::fromCorePage(*page)) {
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
            if (RefPtr extensionControllerProxy = webPage->webExtensionControllerProxy())
                parameters.pageHasLoadedWebExtensions = extensionControllerProxy->hasLoadedContexts();
#endif
        }
    }

    if (RefPtr ownerElement = frame->ownerElement()) {
        if (RefPtr parentFrame = ownerElement->document().frame()) {
            parameters.parentFrameID = parentFrame->loader().frameID();
            parameters.parentCrossOriginEmbedderPolicy = ownerElement->document().crossOriginEmbedderPolicy();
            parameters.parentFrameURL = ownerElement->document().url();
        }
    }

    auto advancedPrivacyProtections = policySourceDocumentLoader ? policySourceDocumentLoader->advancedPrivacyProtections() : OptionSet<AdvancedPrivacyProtections> { };
    parameters.advancedPrivacyProtections = advancedPrivacyProtections;

    if (isMainFrameNavigation)
        parameters.linkPreconnectEarlyHintsEnabled = mainFrame->settings().linkPreconnectEarlyHintsEnabled();
}

void WebLoaderStrategy::scheduleLoadFromNetworkProcess(ResourceLoader& resourceLoader, const ResourceRequest& request, const WebResourceLoader::TrackingParameters& trackingParameters, bool shouldClearReferrerOnHTTPSToHTTPRedirect, Seconds maximumBufferingTime)
{
    auto identifier = *resourceLoader.identifier();

    RefPtr frame = resourceLoader.frame();

    if (RefPtr page = frame ? frame->page() : nullptr) {
        auto mainFrameMainResource = frame->isMainFrame()
            && resourceLoader.frameLoader()
            && resourceLoader.frameLoader()->notifier().isInitialRequestIdentifier(identifier)
            ? MainFrameMainResource::Yes : MainFrameMainResource::No;
        if (!page->allowsLoadFromURL(request.url(), mainFrameMainResource)) {
            RunLoop::mainSingleton().dispatch([resourceLoader = Ref { resourceLoader }, error = blockedError(request)] {
                resourceLoader->didFail(error);
            });
            return;
        }
    }

    ContentSniffingPolicy contentSniffingPolicy = resourceLoader.shouldSniffContent() ? ContentSniffingPolicy::SniffContent : ContentSniffingPolicy::DoNotSniffContent;
    auto contentEncodingSniffingPolicy = resourceLoader.contentEncodingSniffingPolicy();
    StoredCredentialsPolicy storedCredentialsPolicy = resourceLoader.shouldUseCredentialStorage() ? StoredCredentialsPolicy::Use : StoredCredentialsPolicy::DoNotUse;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, url '%s' will be scheduled with the NetworkProcess with priority %d, storedCredentialsPolicy %i", resourceLoader.url().string().latin1().data(), static_cast<int>(resourceLoader.request().priority()), (int)storedCredentialsPolicy);

    NetworkResourceLoadParameters loadParameters {
        trackingParameters.webPageProxyID,
        trackingParameters.pageID,
        trackingParameters.frameID,
        request
    };
    loadParameters.createSandboxExtensionHandlesIfNecessary();

    loadParameters.identifier = identifier;
    loadParameters.parentPID = legacyPresentingApplicationPID();
    loadParameters.contentSniffingPolicy = contentSniffingPolicy;
    loadParameters.contentEncodingSniffingPolicy = contentEncodingSniffingPolicy;
    loadParameters.storedCredentialsPolicy = storedCredentialsPolicy;
    // If there is no WebFrame then this resource cannot be authenticated with the client.
    loadParameters.clientCredentialPolicy = resourceLoader.isAllowedToAskUserForCredentials() ? ClientCredentialPolicy::MayAskClientForCredentials : ClientCredentialPolicy::CannotAskClientForCredentials;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect;
    loadParameters.needsCertificateInfo = resourceLoader.shouldIncludeCertificateInfo();
    loadParameters.maximumBufferingTime = maximumBufferingTime;
    loadParameters.options = resourceLoader.options();
    loadParameters.preflightPolicy = resourceLoader.options().preflightPolicy;
    bool isMainFrameNavigation = resourceLoader.frame() && resourceLoader.frame()->isMainFrame() && resourceLoader.options().mode == FetchOptions::Mode::Navigate;
    addParametersShared(frame.get(), loadParameters, isMainFrameNavigation);

    loadParameters.serviceWorkersMode = resourceLoader.options().loadedFromOpaqueSource == LoadedFromOpaqueSource::No ? resourceLoader.options().serviceWorkersMode : ServiceWorkersMode::None;
    loadParameters.serviceWorkerRegistrationIdentifier = resourceLoader.options().serviceWorkerRegistrationIdentifier;
    loadParameters.workerIdentifier = resourceLoader.options().workerIdentifier;
    loadParameters.httpHeadersToKeep = resourceLoader.options().httpHeadersToKeep;
    if (resourceLoader.options().navigationPreloadIdentifier)
        loadParameters.navigationPreloadIdentifier = resourceLoader.options().navigationPreloadIdentifier;
    if (frame && !frame->isMainFrame())
        loadParameters.shouldRecordFrameLoadForStorageAccess = frame->settings().requestStorageAccessThrowsExceptionUntilReload();

    RefPtr document = frame ? frame->document() : nullptr;
    if (resourceLoader.options().cspResponseHeaders)
        loadParameters.cspResponseHeaders = resourceLoader.options().cspResponseHeaders;
    else if (document && !document->shouldBypassMainWorldContentSecurityPolicy() && resourceLoader.options().contentSecurityPolicyImposition == ContentSecurityPolicyImposition::DoPolicyCheck) {
        if (CheckedPtr contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }

    if (resourceLoader.options().crossOriginEmbedderPolicy)
        loadParameters.crossOriginEmbedderPolicy = *resourceLoader.options().crossOriginEmbedderPolicy;
    
    auto* webFrameLoaderClient = frame ? dynamicDowncast<WebLocalFrameLoaderClient>(frame->loader().client()) : nullptr;
    RefPtr webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;

#if ENABLE(APP_BOUND_DOMAINS)
    if (webFrame)
        loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif

    if (document) {
        loadParameters.frameURL = document->url();
#if ENABLE(CONTENT_EXTENSIONS) || (ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS))
    if (RefPtr page = document->page())
        loadParameters.mainDocumentURL = page->mainFrameURL();
#endif
#if ENABLE(CONTENT_EXTENSIONS)
        // FIXME: Instead of passing userContentControllerIdentifier, the NetworkProcess should be able to get it using webPageId.
        if (RefPtr webPage = webFrame ? webFrame->page() : nullptr)
            loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
#endif
    }

    // FIXME: All loaders should provide their origin if navigation mode is cors/no-cors/same-origin.
    // As a temporary approach, we use the document origin if available or the HTTP Origin header otherwise.
    if (auto* loader = dynamicDowncast<SubresourceLoader>(resourceLoader)) {
        loadParameters.sourceOrigin = loader->origin();

        if (auto* headers = loader->originalHeaders())
            loadParameters.originalRequestHeaders = *headers;
    }

    if (!loadParameters.sourceOrigin && document)
        loadParameters.sourceOrigin = document->securityOrigin();
    if (!loadParameters.sourceOrigin) {
        auto origin = request.httpOrigin();
        if (!origin.isNull())
            loadParameters.sourceOrigin = SecurityOrigin::createFromString(origin);
    }
    if (isMainFrameNavigation) {
        if (request.url().protocolIsBlob() && resourceLoader.documentLoader() && !resourceLoader.documentLoader()->triggeringAction().isEmpty() && resourceLoader.documentLoader()->triggeringAction().requester())
            loadParameters.topOrigin = resourceLoader.documentLoader()->triggeringAction().requester()->topOrigin.ptr();
        else
            loadParameters.topOrigin = SecurityOrigin::create(request.url());
    } else if (document)
        loadParameters.topOrigin = document->topOrigin();

    if (document)
        loadParameters.documentURL = document->url();

    if (loadParameters.options.mode != FetchOptions::Mode::Navigate) {
        ASSERT(loadParameters.sourceOrigin);
        if (!loadParameters.sourceOrigin) {
            WEBLOADERSTRATEGY_RELEASE_LOG_ERROR("scheduleLoad: no sourceOrigin (priority=%d)", static_cast<int>(resourceLoader.request().priority()));
            scheduleInternallyFailedLoad(resourceLoader);
            return;
        }
    }

    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.isMainFrameNavigation = isMainFrameNavigation;
    if (loadParameters.isMainFrameNavigation && document) {
        // Fall back to use opener's cross-origin opener policy like in Document::initSecurityContext.
        RefPtr webFrame = WebFrame::webFrame(frame->frameID());
        RefPtr coreFrame = webFrame ? webFrame->coreFrame() : nullptr;
        RefPtr openerFrame = coreFrame ? coreFrame->opener() : nullptr;
        RefPtr openerDocumentSecurityOrigin = openerFrame ? openerFrame->frameDocumentSecurityOrigin() : nullptr;
        bool openerDocumentIsSameOriginAsTopDocument = openerDocumentSecurityOrigin ? openerDocumentSecurityOrigin->isSameOriginAs(openerFrame->protectedTopOrigin()) : false;
        auto openerDocumentSecurityPolicy = openerFrame ? openerFrame->frameDocumentSecurityPolicy() : std::nullopt;
        if (!document->haveInitializedSecurityOrigin() && openerDocumentSecurityPolicy && openerDocumentIsSameOriginAsTopDocument)
            loadParameters.sourceCrossOriginOpenerPolicy = openerDocumentSecurityPolicy->crossOriginOpenerPolicy;
        else
            loadParameters.sourceCrossOriginOpenerPolicy = document->crossOriginOpenerPolicy();
    }

    if (resourceLoader.frame()
        && resourceLoader.options().mode == FetchOptions::Mode::Navigate
        && webFrame
        && webFrame->frameLoaderClient()) {
        auto navigationUpgradeToHTTPSBehavior = frame ? frame->loader().navigationUpgradeToHTTPSBehavior() : NavigationUpgradeToHTTPSBehavior::BasedOnPolicy;
        // FIXME: Gather more parameters here like we have in WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction.
        loadParameters.mainResourceNavigationDataForAnyFrame = webFrame->frameLoaderClient()->navigationActionData(resourceLoader.documentLoader()->triggeringAction(), request, { }, { }, { }, { }, { }, navigationUpgradeToHTTPSBehavior, { });
    }
    if (loadParameters.mainResourceNavigationDataForAnyFrame) {
        if (RefPtr documentLoader = resourceLoader.documentLoader()) {
            loadParameters.navigationID = documentLoader->navigationID();
            loadParameters.navigationRequester = documentLoader->triggeringAction().requester();
        }
    }
    loadParameters.isCrossOriginOpenerPolicyEnabled = document && document->settings().crossOriginOpenerPolicyEnabled();
    loadParameters.isDisplayingInitialEmptyDocument = frame && frame->loader().stateMachine().isDisplayingInitialEmptyDocument();
    if (frame)
        loadParameters.effectiveSandboxFlags = frame->sandboxFlagsFromSandboxAttributeNotCSP();
    if (auto* openerFrame = frame ? dynamicDowncast<LocalFrame>(frame->opener()) : nullptr) {
        if (auto openerDocument = openerFrame->document())
            loadParameters.openerURL = openerDocument->url();
    }

    loadParameters.shouldEnableCrossOriginResourcePolicy = !loadParameters.isMainFrameNavigation;

    if (resourceLoader.options().mode == FetchOptions::Mode::Navigate) {
        Vector<Ref<SecurityOrigin>> frameAncestorOrigins;

        // Use the WebFrame to get the parent because this may be a provisional frame not hooked up to its parent yet.
        RefPtr thisWebFrame = WebFrame::webFrame(frame ? std::optional(frame->frameID()) : std::nullopt);
        RefPtr parentWebFrame = thisWebFrame ? thisWebFrame->parentFrame() : nullptr;
        for (RefPtr frame = parentWebFrame ? parentWebFrame->coreFrame() : nullptr; frame; frame = frame->tree().parent()) {
            RefPtr<WebCore::SecurityOrigin> frameOrigin = frame->frameDocumentSecurityOrigin();
            if (!frameOrigin) {
                WEBLOADERSTRATEGY_RELEASE_LOG_ERROR("scheduleLoad: Unable to get document origin of frame (frameID=%" PRIu64 ")", frame->frameID().toUInt64());
                frameOrigin = SecurityOrigin::opaqueOrigin();
            }
            frameAncestorOrigins.append(*frameOrigin);
        }
        loadParameters.frameAncestorOrigins = WTF::move(frameAncestorOrigins);
    }

    if (RefPtr frameLoader = resourceLoader.frameLoader())
        loadParameters.requiredCookiesVersion = frameLoader->requiredCookiesVersion();

    if (CachedResourceHandle handle = resourceLoader.cachedResource())
        loadParameters.isInitiatorPrefetch = handle->type() == CachedResource::Type::LinkPrefetch;

    std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume;
    if (loadParameters.isMainFrameNavigation)
        existingNetworkResourceLoadIdentifierToResume = std::exchange(m_existingNetworkResourceLoadIdentifierToResume, std::nullopt);
    WEBLOADERSTRATEGY_RELEASE_LOG_FORWARDABLE(WEBLOADERSTRATEGY_SCHEDULELOAD_RESOURCE_SCHEDULED_WITH_NETWORKPROCESS, static_cast<int>(resourceLoader.request().priority()), existingNetworkResourceLoadIdentifierToResume ? existingNetworkResourceLoadIdentifierToResume->toUInt64() : 0);

    loadParameters.isInitiatedByDedicatedWorker = resourceLoader.options().initiatorContext == InitiatorContext::Worker && std::holds_alternative<std::monostate>(resourceLoader.options().workerIdentifier);
    if (WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(WTF::move(loadParameters), existingNetworkResourceLoadIdentifierToResume), 0) != IPC::Error::NoError) {
        WEBLOADERSTRATEGY_RELEASE_LOG_ERROR("scheduleLoad: Unable to schedule resource with the NetworkProcess (priority=%d)", static_cast<int>(resourceLoader.request().priority()));
        // We probably failed to schedule this load with the NetworkProcess because it had crashed.
        // This load will never succeed so we will schedule it to fail asynchronously.
        scheduleInternallyFailedLoad(resourceLoader);
        return;
    }

    auto loader = WebResourceLoader::create(resourceLoader, trackingParameters);
    m_webResourceLoaders.set(identifier, WTF::move(loader));
}

void WebLoaderStrategy::scheduleInternallyFailedLoad(WebCore::ResourceLoader& resourceLoader)
{
    m_internallyFailedResourceLoaders.add(&resourceLoader);
    m_internallyFailedLoadTimer.startOneShot(0_s);
}

void WebLoaderStrategy::internallyFailedLoadTimerFired()
{
    for (auto& resourceLoader : copyToVector(m_internallyFailedResourceLoaders))
        resourceLoader->didFail(internalError(resourceLoader->url()));
}

void WebLoaderStrategy::startLocalLoad(WebCore::ResourceLoader& resourceLoader)
{
    resourceLoader.start();
    m_webResourceLoaders.set(*resourceLoader.identifier(), WebResourceLoader::create(resourceLoader, { }));
}

void WebLoaderStrategy::addURLSchemeTaskProxy(WebURLSchemeTaskProxy& task)
{
    auto result = m_urlSchemeTasks.add(task.identifier(), task);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void WebLoaderStrategy::removeURLSchemeTaskProxy(WebURLSchemeTaskProxy& task)
{
    m_urlSchemeTasks.remove(task.identifier());
}

void WebLoaderStrategy::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);
    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::remove, url '%s'", resourceLoader->url().string().utf8().data());

    auto identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebLoaderStrategy removing a ResourceLoader that has no identifier.");
        return;
    }

    if (RefPtr task = m_urlSchemeTasks.take(*identifier).get()) {
        ASSERT(!m_internallyFailedResourceLoaders.contains(resourceLoader));
        task->stopLoading();
        return;
    }

    if (m_internallyFailedResourceLoaders.contains(resourceLoader)) {
        m_internallyFailedResourceLoaders.remove(resourceLoader);
        return;
    }

    RefPtr<WebResourceLoader> loader = m_webResourceLoaders.take(*identifier);
    // Loader may not be registered if we created it, but haven't scheduled yet (a bundle client can decide to cancel such request via willSendRequest).
    if (!loader)
        return;

    if (RefPtr networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(*identifier), 0);

    // It's possible that this WebResourceLoader might be just about to message back to the NetworkProcess (e.g. ContinueWillSendRequest)
    // but there's no point in doing so anymore.
    loader->detachFromCoreLoader();
}

void WebLoaderStrategy::setDefersLoading(ResourceLoader&, bool)
{
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
    WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC("networkProcessCrashed: failing all pending resource loaders");

    for (auto& loader : m_webResourceLoaders.values()) {
        scheduleInternallyFailedLoad(*protect(loader->resourceLoader()));
        loader->detachFromCoreLoader();
    }

    m_webResourceLoaders.clear();

    auto pingLoadCompletionHandlers = WTF::move(m_pingLoadCompletionHandlers);
    for (auto& pingLoadCompletionHandler : pingLoadCompletionHandlers.values())
        pingLoadCompletionHandler(internalError(URL()), { });

    auto preconnectCompletionHandlers = WTF::move(m_preconnectCompletionHandlers);
    for (auto& preconnectCompletionHandler : preconnectCompletionHandlers.values())
        preconnectCompletionHandler(internalError(URL()));
}

static bool shouldClearReferrerOnHTTPSToHTTPRedirect(LocalFrame* frame)
{
    if (frame) {
        if (RefPtr document = frame->document())
            return document->referrerPolicy() == ReferrerPolicy::NoReferrerWhenDowngrade;
    }
    return true;
}

WebLoaderStrategy::SyncLoadResult WebLoaderStrategy::loadDataURLSynchronously(const ResourceRequest& request)
{
    SyncLoadResult result;
    auto decodeResult = DataURLDecoder::decode(request.url());
    if (!decodeResult) {
        WEBLOADERSTRATEGY_RELEASE_LOG_BASIC("loadDataURLSynchronously: decoding of data failed");
        result.error = internalError(request.url());
        return result;
    }

    result.response = ResourceResponse::dataURLResponse(request.url(), decodeResult.value());
    result.data = WTF::move(decodeResult->data);

    return result;
}

std::optional<WebLoaderStrategy::SyncLoadResult> WebLoaderStrategy::tryLoadingSynchronouslyUsingURLSchemeHandler(FrameLoader& frameLoader, WebCore::ResourceLoaderIdentifier identifier, const ResourceRequest& request)
{
    auto* webFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(frameLoader.client());
    auto* webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;
    RefPtr webPage = webFrame ? webFrame->page() : nullptr;
    if (!webPage)
        return std::nullopt;

    RefPtr handler = webPage->urlSchemeHandlerForScheme(request.url().protocol());
    if (!handler)
        return std::nullopt;

    LOG(NetworkScheduling, "(WebProcess) WebLoaderStrategy::scheduleLoad, sync load to URL '%s' will be handled by a UIProcess URL scheme handler.", request.url().string().utf8().data());

    SyncLoadResult result;
    handler->loadSynchronously(identifier, *webFrame, request, result.response, result.error, result.data);

    return result;
}

void WebLoaderStrategy::loadResourceSynchronously(FrameLoader& frameLoader, WebCore::ResourceLoaderIdentifier resourceLoadIdentifier, const ResourceRequest& request, ClientCredentialPolicy clientCredentialPolicy,  const FetchOptions& options, const HTTPHeaderMap& originalRequestHeaders, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    RefPtr webFrameLoaderClient = dynamicDowncast<WebLocalFrameLoaderClient>(frameLoader.client());
    RefPtr webFrame = webFrameLoaderClient ? &webFrameLoaderClient->webFrame() : nullptr;
    if (!webFrame) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr webPage = webFrame ? webFrame->page() : nullptr;
    if (!webPage) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr page = webPage ? webPage->corePage() : nullptr;
    if (!page) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto webPageProxyID = webPage->webPageProxyIdentifier();
    auto pageID = webPage->identifier();
    auto frameID = webFrame->frameID();

    [[maybe_unused]] WebResourceLoader::TrackingParameters trackingParameters {
        webPageProxyID,
        pageID,
        frameID,
        resourceLoadIdentifier
    };

    RefPtr document = frameLoader.frame().document();
    if (!document) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: no document");
        error = internalError(request.url());
        return;
    }

    if (request.url().protocolIsData()) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: URL will be loaded as data");
        auto syncLoadResult = loadDataURLSynchronously(request);
        error = WTF::move(syncLoadResult.error);
        response = WTF::move(syncLoadResult.response);
        data = WTF::move(syncLoadResult.data);
        return;
    }

    if (auto syncLoadResult = tryLoadingSynchronouslyUsingURLSchemeHandler(frameLoader, resourceLoadIdentifier, request)) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: failed calling tryLoadingSynchronouslyUsingURLSchemeHandler (error=%d)", syncLoadResult->error.errorCode());
        error = WTF::move(syncLoadResult->error);
        response = WTF::move(syncLoadResult->response);
        data = WTF::move(syncLoadResult->data);
        return;
    }

    NetworkResourceLoadParameters loadParameters {
        webPageProxyID,
        pageID,
        frameID,
        request
    };
    loadParameters.createSandboxExtensionHandlesIfNecessary();

    loadParameters.identifier = resourceLoadIdentifier;
    loadParameters.parentPID = legacyPresentingApplicationPID();
    loadParameters.contentSniffingPolicy = ContentSniffingPolicy::SniffContent;
    loadParameters.contentEncodingSniffingPolicy = ContentEncodingSniffingPolicy::Default;
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.clientCredentialPolicy = clientCredentialPolicy;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(webFrame ? protect(webFrame->coreLocalFrame()).get() : nullptr);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();

    loadParameters.options = options;
    loadParameters.sourceOrigin = document->securityOrigin();
    loadParameters.topOrigin = document->topOrigin();
    if (!document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (CheckedPtr contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }
    loadParameters.originalRequestHeaders = originalRequestHeaders;
#if ENABLE(APP_BOUND_DOMAINS)
    if (webFrame)
        loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif
    addParametersShared(protect(webFrame->coreLocalFrame()).get(), loadParameters);

    data.shrink(0);

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::PerformSynchronousLoad(WTF::move(loadParameters)), 0);
    if (!sendResult.succeeded()) {
        WEBLOADERSTRATEGY_WITH_FRAMELOADER_RELEASE_LOG_ERROR("loadResourceSynchronously: failed sending synchronous network process message %" PUBLIC_LOG_STRING, IPC::errorAsString(sendResult.error()).characters());
        if (page)
            page->checkedDiagnosticLoggingClient()->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::synchronousMessageFailedKey(), WebCore::ShouldSample::No);
        response = ResourceResponse();
        error = internalError(request.url());
    } else
        std::tie(error, response, data) = sendResult.takeReply();
}

void WebLoaderStrategy::pageLoadCompleted(Page& page)
{
    if (RefPtr networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection())
        networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::PageLoadCompleted(WebPage::fromCorePage(page)->identifier()), 0);
}

void WebLoaderStrategy::browsingContextRemoved(LocalFrame& frame)
{
    ASSERT(frame.page());
    RefPtr networkProcessConnection = WebProcess::singleton().existingNetworkProcessConnection();
    if (!networkProcessConnection)
        return;

    Ref page = *WebPage::fromCorePage(*protect(frame.page()));
    networkProcessConnection->connection().send(Messages::NetworkConnectionToWebProcess::BrowsingContextRemoved(page->webPageProxyIdentifier(), page->identifier(), WebFrame::fromCoreFrame(frame)->frameID()), 0);
}

void WebLoaderStrategy::startPingLoad(LocalFrame& frame, ResourceRequest& request, const HTTPHeaderMap& originalRequestHeaders, const FetchOptions& options, ContentSecurityPolicyImposition policyCheck, PingLoadCompletionHandler&& completionHandler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    RefPtr document = frame.document();
    if (!document || !webFrame) {
        if (completionHandler)
            completionHandler(internalError(request.url()), { });
        return;
    }

    RefPtr webPage = webFrame->page();
    if (!webPage) {
        if (completionHandler)
            completionHandler(internalError(request.url()), { });
        return;
    }

    NetworkResourceLoadParameters loadParameters {
        webPage->webPageProxyIdentifier(),
        webPage->identifier(),
        webFrame->frameID(),
        request
    };
    loadParameters.createSandboxExtensionHandlesIfNecessary();

    loadParameters.identifier = WebCore::ResourceLoaderIdentifier::generate();
    loadParameters.sourceOrigin = document->securityOrigin();
    loadParameters.topOrigin = document->topOrigin();
    loadParameters.parentPID = legacyPresentingApplicationPID();
    loadParameters.storedCredentialsPolicy = options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use;
    loadParameters.options = options;
    loadParameters.originalRequestHeaders = originalRequestHeaders;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = shouldClearReferrerOnHTTPSToHTTPRedirect(&frame);
    loadParameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    if (policyCheck == ContentSecurityPolicyImposition::DoPolicyCheck && !document->shouldBypassMainWorldContentSecurityPolicy()) {
        if (CheckedPtr contentSecurityPolicy = document->contentSecurityPolicy())
            loadParameters.cspResponseHeaders = contentSecurityPolicy->responseHeaders();
    }
    addParametersShared(&frame, loadParameters);
#if ENABLE(APP_BOUND_DOMAINS)
    loadParameters.isNavigatingToAppBoundDomain = webFrame->isTopFrameNavigatingToAppBoundDomain();
#endif

    loadParameters.frameURL = document->url();
#if ENABLE(CONTENT_EXTENSIONS) || (ENABLE(CONTENT_FILTERING) && HAVE(WEBCONTENTRESTRICTIONS))
    if (RefPtr page = document->page())
        loadParameters.mainDocumentURL = page->mainFrameURL();
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    // FIXME: Instead of passing userContentControllerIdentifier, we should just pass webPageId to NetworkProcess.
    loadParameters.userContentControllerIdentifier = webPage->userContentControllerIdentifier();
#endif

    if (completionHandler)
        m_pingLoadCompletionHandlers.add(*loadParameters.identifier, WTF::move(completionHandler));

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::LoadPing { WTF::move(loadParameters) }, 0);
}

void WebLoaderStrategy::didFinishPingLoad(WebCore::ResourceLoaderIdentifier pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    if (auto completionHandler = m_pingLoadCompletionHandlers.take(pingLoadIdentifier))
        completionHandler(WTF::move(error), WTF::move(response));
}

void WebLoaderStrategy::preconnectTo(FrameLoader& frameLoader, ResourceRequest&& request, StoredCredentialsPolicy storedCredentialsPolicy, ShouldPreconnectAsFirstParty shouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameLoader.frameID());
    if (!webFrame)
        return completionHandler(internalError(request.url()));

    RefPtr webPage = webFrame->page();
    if (!webPage)
        return completionHandler(internalError(request.url()));

    preconnectTo(WTF::move(request), *webPage, *webFrame, storedCredentialsPolicy, shouldPreconnectAsFirstParty, WTF::move(completionHandler));
}

void WebLoaderStrategy::preconnectTo(WebCore::ResourceRequest&& request, WebPage& webPage, WebFrame& webFrame, WebCore::StoredCredentialsPolicy storedCredentialsPolicy, ShouldPreconnectAsFirstParty shouldPreconnectAsFirstParty, PreconnectCompletionHandler&& completionHandler)
{
    if (RefPtr corePage = webPage.corePage(); corePage && !corePage->allowsLoadFromURL(request.url(), MainFrameMainResource::No)) {
        if (completionHandler)
            completionHandler({ });
        return;
    }

    RefPtr mainFrame = dynamicDowncast<WebCore::LocalFrame>(webPage.mainFrame());
    if (RefPtr document = mainFrame ? mainFrame->document() : nullptr) {
        if (shouldPreconnectAsFirstParty == ShouldPreconnectAsFirstParty::Yes)
            request.setFirstPartyForCookies(request.url());
        else
            request.setFirstPartyForCookies(document->firstPartyForCookies());
        if (RefPtr loader = document->loader())
            request.setIsAppInitiated(loader->lastNavigationWasAppInitiated());
    }

    NetworkResourceLoadParameters parameters {
        webPage.webPageProxyIdentifier(),
        webPage.identifier(),
        webFrame.frameID(),
        WTF::move(request)
    };
    parameters.createSandboxExtensionHandlesIfNecessary();

    if (parameters.request.httpUserAgent().isEmpty()) {
        // FIXME: we add user-agent to the preconnect request because otherwise the preconnect
        // gets thrown away by CFNetwork when using an HTTPS proxy (<rdar://problem/59434166>).
        String webPageUserAgent = webPage.userAgent(parameters.request.url());
        if (!webPageUserAgent.isEmpty())
            parameters.request.setHTTPUserAgent(webPageUserAgent);
    }
    parameters.identifier = WebCore::ResourceLoaderIdentifier::generate();
    parameters.parentPID = legacyPresentingApplicationPID();
    parameters.storedCredentialsPolicy = storedCredentialsPolicy;
    parameters.shouldPreconnectOnly = PreconnectOnly::Yes;
    parameters.shouldRestrictHTTPResponseAccess = shouldPerformSecurityChecks();
    // FIXME: Use the proper destination once all fetch options are passed.
    parameters.options.destination = FetchOptions::Destination::EmptyString;
#if ENABLE(APP_BOUND_DOMAINS)
    parameters.isNavigatingToAppBoundDomain = webFrame.isTopFrameNavigatingToAppBoundDomain();
#endif
    if (RefPtr loader = policySourceDocumentLoaderForFrame(*protect(webFrame.coreLocalFrame())))
        parameters.advancedPrivacyProtections = loader->advancedPrivacyProtections();

    std::optional<WebCore::ResourceLoaderIdentifier> preconnectionIdentifier;
    if (completionHandler) {
        preconnectionIdentifier = parameters.identifier;
        auto addResult = m_preconnectCompletionHandlers.add(*preconnectionIdentifier, WTF::move(completionHandler));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    // FIXME: Use sendWithAsyncReply instead of preconnectionIdentifier
    // FIXME: don't use WebCore::ResourceLoaderIdentifier for a preconnection identifier, too. It should have its own type.
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::PreconnectTo(preconnectionIdentifier, WTF::move(parameters)), 0);
}

void WebLoaderStrategy::didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, ResourceError&& error)
{
    if (auto completionHandler = m_preconnectCompletionHandlers.take(preconnectionIdentifier))
        completionHandler(WTF::move(error));
}

bool WebLoaderStrategy::isOnLine() const
{
    return m_isOnLine;
}

void WebLoaderStrategy::addOnlineStateChangeListener(Function<void(bool)>&& listener)
{
    WebProcess::singleton().ensureNetworkProcessConnection();
    m_onlineStateChangeListeners.append(WTF::move(listener));
}

void WebLoaderStrategy::isResourceLoadFinished(CachedResource& resource, CompletionHandler<void(bool)>&& callback)
{
    if (!resource.loader()) {
        callback(true);
        return;
    }

    if (!WebProcess::singleton().existingNetworkProcessConnection()) {
        callback(true);
        return;
    }

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::IsResourceLoadFinished(*resource.loader()->identifier()), WTF::move(callback), 0);
}

void WebLoaderStrategy::setOnLineState(bool isOnLine)
{
    if (m_isOnLine == isOnLine)
        return;

    m_isOnLine = isOnLine;
    for (auto& listener : m_onlineStateChangeListeners)
        listener(isOnLine);
}

void WebLoaderStrategy::setCaptureExtraNetworkLoadMetricsEnabled(bool enabled)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCaptureExtraNetworkLoadMetricsEnabled(enabled), 0);
}

ResourceResponse WebLoaderStrategy::responseFromResourceLoadIdentifier(ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadInformationResponse { resourceLoadIdentifier }, 0);
    auto [response] = sendResult.takeReplyOr(ResourceResponse { });
    return response;
}

Vector<NetworkTransactionInformation> WebLoaderStrategy::intermediateLoadInformationFromResourceLoadIdentifier(WebCore::ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetNetworkLoadIntermediateInformation { resourceLoadIdentifier }, 0);
    auto [information] = sendResult.takeReplyOr(Vector<NetworkTransactionInformation> { });
    return information;
}

NetworkLoadMetrics WebLoaderStrategy::networkMetricsFromResourceLoadIdentifier(WebCore::ResourceLoaderIdentifier resourceLoadIdentifier)
{
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TakeNetworkLoadInformationMetrics { resourceLoadIdentifier }, 0);
    auto [networkMetrics] = sendResult.takeReplyOr(NetworkLoadMetrics { });
    return networkMetrics;
}

bool WebLoaderStrategy::shouldPerformSecurityChecks() const
{
    return true;
}

bool WebLoaderStrategy::havePerformedSecurityChecks(const ResourceResponse& response) const
{
    if (!shouldPerformSecurityChecks())
        return false;
    switch (response.source()) {
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::LegacyApplicationCachePlaceholder:
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
    case ResourceResponse::Source::ServiceWorker:
    case ResourceResponse::Source::InspectorOverride:
        return false;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
    case ResourceResponse::Source::Network:
    case ResourceResponse::Source::Unknown:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void WebLoaderStrategy::setResourceLoadSchedulingMode(WebCore::Page& page, WebCore::LoadSchedulingMode mode)
{
    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.send(Messages::NetworkConnectionToWebProcess::SetResourceLoadSchedulingMode(WebPage::fromCorePage(page)->identifier(), mode), 0);
}

void WebLoaderStrategy::prioritizeResourceLoads(const Vector<Ref<WebCore::SubresourceLoader>>& resources)
{
    auto identifiers = resources.map([](auto& loader) {
        return *loader->identifier();
    });

    auto& connection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
    connection.send(Messages::NetworkConnectionToWebProcess::PrioritizeResourceLoads(identifiers), 0);
}

ResourceError WebLoaderStrategy::cancelledError(const ResourceRequest& request) const
{
    return WebKit::cancelledError(request);
}

ResourceError WebLoaderStrategy::blockedError(const ResourceRequest& request) const
{
    return WebKit::blockedError(request);
}

ResourceError WebLoaderStrategy::blockedByContentBlockerError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentBlockerError(request);
}

ResourceError WebLoaderStrategy::cannotShowURLError(const ResourceRequest& request) const
{
    return WebKit::cannotShowURLError(request);
}

ResourceError WebLoaderStrategy::interruptedForPolicyChangeError(const ResourceRequest& request) const
{
    return WebKit::interruptedForPolicyChangeError(request);
}

#if ENABLE(CONTENT_FILTERING)
ResourceError WebLoaderStrategy::blockedByContentFilterError(const ResourceRequest& request) const
{
    return WebKit::blockedByContentFilterError(request);
}
#endif

ResourceError WebLoaderStrategy::cannotShowMIMETypeError(const ResourceResponse& response) const
{
    return WebKit::cannotShowMIMETypeError(response);
}

ResourceError WebLoaderStrategy::fileDoesNotExistError(const ResourceResponse& response) const
{
    return WebKit::fileDoesNotExistError(response);
}

ResourceError WebLoaderStrategy::httpsUpgradeRedirectLoopError(const ResourceRequest& request) const
{
    return WebKit::httpsUpgradeRedirectLoopError(request);
}

ResourceError WebLoaderStrategy::httpNavigationWithHTTPSOnlyError(const ResourceRequest& request) const
{
    return WebKit::httpNavigationWithHTTPSOnlyError(request);
}

ResourceError WebLoaderStrategy::pluginWillHandleLoadError(const ResourceResponse& response) const
{
    return WebKit::pluginWillHandleLoadError(response);
}

} // namespace WebKit

#undef WEBLOADERSTRATEGY_RELEASE_LOG_BASIC
#undef WEBLOADERSTRATEGY_RELEASE_LOG_ERROR_BASIC
