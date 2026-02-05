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
#include "WebResourceLoader.h"

#include "FormDataReference.h"
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoaderMessages.h"
#include "PrivateRelayed.h"
#include "SharedBufferReference.h"
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebLoaderStrategy.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebURLSchemeHandlerProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/InspectorInstrumentationWebKit.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/SubresourceLoader.h>
#include <WebCore/SubstituteData.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/MakeString.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include <WebCore/ResourceMonitor.h>
#endif


#define WEBRESOURCELOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG_FORWARDABLE(Network, fmt, m_trackingParameters ? m_trackingParameters->pageID.toUInt64() : 0, m_trackingParameters ? m_trackingParameters->frameID.toUInt64() : 0, m_trackingParameters ? m_trackingParameters->resourceID.toUInt64() : 0, ##__VA_ARGS__)

namespace WebKit {
using namespace WebCore;

Ref<WebResourceLoader> WebResourceLoader::create(Ref<ResourceLoader>&& coreLoader, const std::optional<TrackingParameters>& trackingParameters)
{
    return adoptRef(*new WebResourceLoader(WTF::move(coreLoader), trackingParameters));
}

WebResourceLoader::WebResourceLoader(Ref<WebCore::ResourceLoader>&& coreLoader, const std::optional<TrackingParameters>& trackingParameters)
    : m_coreLoader(WTF::move(coreLoader))
    , m_trackingParameters(trackingParameters)
    , m_loadStart(MonotonicTime::now())
{
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_CONSTRUCTOR);
}

WebResourceLoader::~WebResourceLoader() = default;

IPC::Connection* WebResourceLoader::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

uint64_t WebResourceLoader::messageSenderDestinationID() const
{
    RELEASE_ASSERT(RunLoop::isMain());
    return protect(resourceLoader())->identifier()->toUInt64();
}

void WebResourceLoader::detachFromCoreLoader()
{
    RELEASE_ASSERT(RunLoop::isMain());
    m_coreLoader = nullptr;
}

MainFrameMainResource WebResourceLoader::mainFrameMainResource() const
{
    RefPtr coreLoader = m_coreLoader;
    RefPtr frame = coreLoader->frame();
    if (!frame || !frame->isMainFrame())
        return MainFrameMainResource::No;

    RefPtr frameLoader = coreLoader->frameLoader();
    if (!frameLoader)
        return MainFrameMainResource::No;

    if (!frameLoader->notifier().isInitialRequestIdentifier(*coreLoader->identifier()))
        return MainFrameMainResource::No;

    return MainFrameMainResource::Yes;
}

void WebResourceLoader::willSendRequest(ResourceRequest&& proposedRequest, IPC::FormDataReference&& proposedRequestBody, ResourceResponse&& redirectResponse, CompletionHandler<void(ResourceRequest&&, bool)>&& completionHandler)
{
    Ref<WebResourceLoader> protectedThis(*this);
    RefPtr coreLoader = m_coreLoader;

    // Make the request whole again as we do not normally encode the request's body when sending it over IPC, for performance reasons.
    proposedRequest.setHTTPBody(proposedRequestBody.takeData());

    LOG(Network, "(WebProcess) WebResourceLoader::willSendRequest to '%s'", proposedRequest.url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_WILLSENDREQUEST);
    
    if (RefPtr frame = coreLoader->frame()) {
        if (RefPtr page = frame->page()) {
            if (!page->allowsLoadFromURL(proposedRequest.url(), mainFrameMainResource()))
                proposedRequest = { };
        }
    }

    coreLoader->willSendRequest(WTF::move(proposedRequest), redirectResponse, [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] (ResourceRequest&& request) mutable {
        RefPtr coreLoader = m_coreLoader;
        if (!m_coreLoader || !coreLoader->identifier()) {
            WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_WILLSENDREQUEST_NO_CORELOADER);
            return completionHandler({ }, false);
        }

        WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_WILLSENDREQUEST_CONTINUE);
        completionHandler(WTF::move(request), coreLoader->isAllowedToAskUserForCredentials());
    });
}

void WebResourceLoader::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    protect(resourceLoader())->didSendData(bytesSent, totalBytesToBeSent);
}

static ASCIILiteral toString(WebCore::RouterSourceEnum source)
{
    switch (source) {
    case WebCore::RouterSourceEnum::Cache:
        return "cache"_s;
    case WebCore::RouterSourceEnum::FetchEvent:
        return "fetch-event"_s;
    case WebCore::RouterSourceEnum::Network:
        return "network"_s;
    case WebCore::RouterSourceEnum::RaceNetworkAndFetchHandler:
        return "race-network-and-fetch-handler"_s;
    }

    ASSERT_NOT_REACHED();
    return ""_s;
}

void WebResourceLoader::updateNetworkLoadMetrics(NetworkLoadMetrics& metrics)
{
    if (!m_serviceWorkerTimingInfo)
        return;

    metrics.workerStart = m_serviceWorkerTimingInfo->workerStart;
    metrics.workerRouterEvaluationStart = m_serviceWorkerTimingInfo->workerRouterEvaluationStart;
    metrics.workerCacheLookupStart = m_serviceWorkerTimingInfo->workerCacheLookupStart;
    if (m_serviceWorkerTimingInfo->workerMatchedRouterSource)
        metrics.workerMatchedRouterSource = toString(*m_serviceWorkerTimingInfo->workerMatchedRouterSource);
    if (m_serviceWorkerTimingInfo->workerFinalRouterSource) {
        ASSERT(*m_serviceWorkerTimingInfo->workerFinalRouterSource != WebCore::RouterSourceEnum::RaceNetworkAndFetchHandler);
        metrics.workerFinalRouterSource = toString(*m_serviceWorkerTimingInfo->workerFinalRouterSource);
    }
}

void WebResourceLoader::didReceiveResponse(ResourceResponse&& response, PrivateRelayed privateRelayed, bool needsContinueDidReceiveResponseMessage, std::optional<NetworkLoadMetrics>&& metrics)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResponse for '%s'. Status %d.", coreLoader->url().string().latin1().data(), response.httpStatusCode());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVERESPONSE, response.httpStatusCode());

    Ref<WebResourceLoader> protectedThis(*this);

    if (metrics) {
        updateNetworkLoadMetrics(*metrics);
        response.setDeprecatedNetworkLoadMetrics(Box<NetworkLoadMetrics>::create(WTF::move(*metrics)));
    }

    if (privateRelayed == PrivateRelayed::Yes && mainFrameMainResource() == MainFrameMainResource::Yes)
        WebProcess::singleton().setHadMainFrameMainResourcePrivateRelayed();

    CompletionHandler<void()> policyDecisionCompletionHandler;
    if (needsContinueDidReceiveResponseMessage) {
#if ASSERT_ENABLED
        m_isProcessingNetworkResponse = true;
#endif
        policyDecisionCompletionHandler = [this, protectedThis = Ref { *this }] {
            RefPtr coreLoader = m_coreLoader;
#if ASSERT_ENABLED
            m_isProcessingNetworkResponse = false;
#endif
            // If coreLoader becomes null as a result of the didReceiveResponse callback, we can't use the send function().
            if (m_coreLoader && coreLoader->identifier())
                send(Messages::NetworkResourceLoader::ContinueDidReceiveResponse());
            else
                WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVERESPONSE_NOT_CONTINUING_LOAD);
        };
    }

    RefPtr frame = coreLoader->frame();
    if (InspectorInstrumentationWebKit::shouldInterceptResponse(frame.get(), response)) {
        auto interceptedRequestIdentifier = *coreLoader->identifier();
        m_interceptController.beginInterceptingResponse(interceptedRequestIdentifier);
        InspectorInstrumentationWebKit::interceptResponse(frame.get(), response, interceptedRequestIdentifier, [this, protectedThis = Ref { *this }, interceptedRequestIdentifier, policyDecisionCompletionHandler = WTF::move(policyDecisionCompletionHandler)](const ResourceResponse& inspectorResponse, RefPtr<FragmentedSharedBuffer> overrideData) mutable {
            RefPtr coreLoader = m_coreLoader;
            if (!m_coreLoader || !coreLoader->identifier()) {
                WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVERESPONSE_NOT_CONTINUING_INTERCEPT_LOAD);
                m_interceptController.continueResponse(interceptedRequestIdentifier);
                return;
            }

            coreLoader->didReceiveResponse(ResourceResponse { inspectorResponse }, [this, protectedThis = Ref { *this }, interceptedRequestIdentifier, policyDecisionCompletionHandler = WTF::move(policyDecisionCompletionHandler), overrideData = WTF::move(overrideData)]() mutable {
                RefPtr coreLoader = m_coreLoader;
                if (policyDecisionCompletionHandler)
                    policyDecisionCompletionHandler();

                if (!m_coreLoader || !coreLoader->identifier()) {
                    m_interceptController.continueResponse(interceptedRequestIdentifier);
                    return;
                }

                if (!overrideData)
                    m_interceptController.continueResponse(interceptedRequestIdentifier);
                else {
                    m_interceptController.interceptedResponse(interceptedRequestIdentifier);
                    if (unsigned bufferSize = overrideData->size())
                        coreLoader->didReceiveBuffer(overrideData.releaseNonNull(), bufferSize, DataPayloadWholeResource);
                    WebCore::NetworkLoadMetrics emptyMetrics;
                    coreLoader->didFinishLoading(emptyMetrics);
                }
            });
        });
        return;
    }

    coreLoader->didReceiveResponse(WTF::move(response), WTF::move(policyDecisionCompletionHandler));
}

void WebResourceLoader::didReceiveData(IPC::SharedBufferReference&& data, uint64_t bytesTransferredOverNetwork)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveData of size %zu for '%s'", data.size(), coreLoader->url().string().latin1().data());
    ASSERT_WITH_MESSAGE(!m_isProcessingNetworkResponse, "Network process should not send data until we've validated the response");

    if (m_interceptController.isIntercepting(*coreLoader->identifier())) [[unlikely]] {
        m_interceptController.defer(*coreLoader->identifier(), [this, protectedThis = Ref { *this }, buffer = WTF::move(data), bytesTransferredOverNetwork]() mutable {
            if (m_coreLoader)
                didReceiveData(WTF::move(buffer), bytesTransferredOverNetwork);
        });
        return;
    }

    bool isFirstDidReceiveData = !m_numBytesReceived;
    if (isFirstDidReceiveData)
        WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVEDATA);
    m_numBytesReceived += data.size();

    auto delta = calculateBytesTransferredOverNetworkDelta(bytesTransferredOverNetwork);

    RefPtr<SharedBuffer> sharedBuffer;
    if (data.isNull())
        sharedBuffer = SharedBuffer::create();
    else if (data.size() < WTF::pageSize() && isFirstDidReceiveData) {
        // For resources less than a page in size, copy the data to a new malloc'd buffer rather
        // than using the entire page sent to us to reduce internal fragmentation.
        //
        // We only do this only for the first data segment. If there are multiple segments in this
        // resource, it will likely end up in a SharedBufferBuilder, which will copy the data when
        // made contiguous.
        sharedBuffer = SharedBuffer::create(data.span());
    } else
        sharedBuffer = data.unsafeBuffer();

    coreLoader->didReceiveData(sharedBuffer.releaseNonNull(), delta, DataPayloadBytes);

#if ENABLE(CONTENT_EXTENSIONS)
    if (delta) {
        if (RefPtr resourceMonitor = coreLoader->resourceMonitorIfExists())
            resourceMonitor->addNetworkUsage(delta);
    }
#endif
}

void WebResourceLoader::didFinishResourceLoad(NetworkLoadMetrics&& networkLoadMetrics)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didFinishResourceLoad for '%s'", coreLoader->url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDFINISHRESOURCELOAD, static_cast<uint64_t>(m_numBytesReceived));

    if (m_interceptController.isIntercepting(*coreLoader->identifier())) [[unlikely]] {
        m_interceptController.defer(*coreLoader->identifier(), [this, protectedThis = Ref { *this }, networkLoadMetrics = WTF::move(networkLoadMetrics)]() mutable {
            if (m_coreLoader)
                didFinishResourceLoad(WTF::move(networkLoadMetrics));
        });
        return;
    }

    updateNetworkLoadMetrics(networkLoadMetrics);
    networkLoadMetrics.markComplete();

#if ENABLE(CONTENT_EXTENSIONS)
    if (networkLoadMetrics.responseBodyBytesReceived != std::numeric_limits<uint64_t>::max()) {
        auto delta = calculateBytesTransferredOverNetworkDelta(networkLoadMetrics.responseBodyBytesReceived);
        if (delta) {
            if (RefPtr resourceMonitor = coreLoader->resourceMonitorIfExists())
                resourceMonitor->addNetworkUsage(delta);
        }
    }
#endif

    ASSERT_WITH_MESSAGE(!m_isProcessingNetworkResponse, "Load should not be able to finish before we've validated the response");
    coreLoader->didFinishLoading(networkLoadMetrics);
}

void WebResourceLoader::didFailServiceWorkerLoad(const ResourceError& error)
{
    RefPtr coreLoader = m_coreLoader;
    if (RefPtr document = coreLoader->frame() ? coreLoader->frame()->document() : nullptr) {
        if (coreLoader->options().destination != FetchOptions::Destination::EmptyString || error.isGeneral())
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, error.localizedDescription());
        if (coreLoader->options().destination != FetchOptions::Destination::EmptyString)
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Error, makeString("Cannot load "_s, error.failingURL().string(), '.'));
    }

    didFailResourceLoad(error);
}

void WebResourceLoader::serviceWorkerDidNotHandle()
{
    RefPtr coreLoader = m_coreLoader;
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_SERVICEWORKERDIDNOTHANDLE);

    ASSERT(coreLoader->options().serviceWorkersMode == ServiceWorkersMode::Only);
    auto error = internalError(coreLoader->request().url());
    error.setType(ResourceError::Type::Cancellation);
    coreLoader->didFail(error);
}

void WebResourceLoader::updateResultingClientIdentifier(WTF::UUID currentIdentifier, WTF::UUID newIdentifier)
{
    if (RefPtr loader = DocumentLoader::fromScriptExecutionContextIdentifier({ currentIdentifier, Process::identifier() }))
        loader->setNewResultingClientId({ newIdentifier, Process::identifier() });
}

void WebResourceLoader::didFailResourceLoad(const ResourceError& error)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didFailResourceLoad for '%s'", coreLoader->url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDFAILRESOURCELOAD);

    if (m_interceptController.isIntercepting(*coreLoader->identifier())) [[unlikely]] {
        m_interceptController.defer(*coreLoader->identifier(), [this, protectedThis = Ref { *this }, error]() mutable {
            if (m_coreLoader)
                didFailResourceLoad(error);
        });
        return;
    }

    ASSERT_WITH_MESSAGE(!m_isProcessingNetworkResponse, "Load should not be able to finish before we've validated the response");

    coreLoader->didFail(error);
}

void WebResourceLoader::didBlockAuthenticationChallenge()
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didBlockAuthenticationChallenge for '%s'", coreLoader->url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDBLOCKAUTHENTICATIONCHALLENGE);

    coreLoader->didBlockAuthenticationChallenge();
}

void WebResourceLoader::stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(const ResourceResponse& response)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied for '%s'", coreLoader->url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_STOPLOADINGAFTERSECURITYPOLICYDENIED);

    protect(coreLoader->documentLoader())->stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(*coreLoader->identifier(), response);
}

#if ENABLE(SHAREABLE_RESOURCE)
void WebResourceLoader::didReceiveResource(ShareableResource::Handle&& handle)
{
    RefPtr coreLoader = m_coreLoader;
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResource for '%s'", coreLoader->url().string().latin1().data());
    WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVERESOURCE);

    RefPtr<SharedBuffer> buffer = WTF::move(handle).tryWrapInSharedBuffer();

    if (!buffer) {
        LOG_ERROR("Unable to create buffer from ShareableResource sent from the network process.");
        WEBRESOURCELOADER_RELEASE_LOG(WEBRESOURCELOADER_DIDRECEIVERESOURCE_UNABLE_TO_CREATE_FRAGMENTEDSHAREDBUFFER);
        if (RefPtr frame = coreLoader->frame()) {
            if (RefPtr page = frame->page())
                page->checkedDiagnosticLoggingClient()->logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::createSharedBufferFailedKey(), WebCore::ShouldSample::No);
        }
        coreLoader->didFail(internalError(coreLoader->request().url()));
        return;
    }

    Ref<WebResourceLoader> protect(*this);

    // Only send data to the didReceiveData callback if it exists.
    if (unsigned bufferSize = buffer->size())
        coreLoader->didReceiveData(buffer.releaseNonNull(), bufferSize, DataPayloadWholeResource);

    if (!m_coreLoader)
        return;

    NetworkLoadMetrics emptyMetrics;
    coreLoader->didFinishLoading(emptyMetrics);
}
#endif

#if ENABLE(CONTENT_FILTERING)
void WebResourceLoader::contentFilterDidBlockLoad(WebCore::ContentFilterUnblockHandler&& unblockHandler, String&& unblockRequestDeniedScript, const ResourceError& error, const URL& blockedPageURL,  WebCore::SubstituteData&& substituteData)
{
    RefPtr coreLoader = m_coreLoader;
    if (!m_coreLoader || !coreLoader->documentLoader())
        return;
    RefPtr documentLoader = coreLoader->documentLoader();
    documentLoader->setBlockedPageURL(blockedPageURL);
    documentLoader->setSubstituteDataFromContentFilter(WTF::move(substituteData));
    documentLoader->handleContentFilterDidBlock(WTF::move(unblockHandler), WTF::move(unblockRequestDeniedScript));
    documentLoader->cancelMainResourceLoad(error);
}
#endif // ENABLE(CONTENT_FILTERING)

size_t WebResourceLoader::calculateBytesTransferredOverNetworkDelta(size_t bytesTransferredOverNetwork)
{
    CheckedSize delta = bytesTransferredOverNetwork - m_bytesTransferredOverNetwork;
    ASSERT(!delta.hasOverflowed());

    m_bytesTransferredOverNetwork = bytesTransferredOverNetwork;
    return delta;
}

} // namespace WebKit

#undef WEBRESOURCELOADER_RELEASE_LOG
