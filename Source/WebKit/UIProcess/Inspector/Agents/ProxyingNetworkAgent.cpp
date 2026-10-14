/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProxyingNetworkAgent.h"

#include "HandleMessage.h"
#include "ProxyingNetworkAgentMessages.h"
#include "WebInspectorBackendMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <WebCore/HTTPHeaderMap.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/NetworkInterception.h>
#include <WebCore/NetworkLoadMetrics.h>
#include <WebCore/ProcessQualified.h>
#include <optional>
#include <tuple>
#include <utility>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/Base64.h>

namespace Inspector {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProxyingNetworkAgent);

static Protocol::Page::ResourceType toProtocolResourceType(ResourceType type)
{
    switch (type) {
    case ResourceType::Document:
        return Protocol::Page::ResourceType::Document;
    case ResourceType::StyleSheet:
        return Protocol::Page::ResourceType::StyleSheet;
    case ResourceType::Image:
        return Protocol::Page::ResourceType::Image;
    case ResourceType::Font:
        return Protocol::Page::ResourceType::Font;
    case ResourceType::Script:
        return Protocol::Page::ResourceType::Script;
    case ResourceType::XHR:
        return Protocol::Page::ResourceType::XHR;
    case ResourceType::Fetch:
        return Protocol::Page::ResourceType::Fetch;
    case ResourceType::Ping:
        return Protocol::Page::ResourceType::Ping;
    case ResourceType::Beacon:
        return Protocol::Page::ResourceType::Beacon;
    case ResourceType::WebSocket:
        return Protocol::Page::ResourceType::WebSocket;
#if ENABLE(APPLICATION_MANIFEST)
    case ResourceType::ApplicationManifest:
        return Protocol::Page::ResourceType::Other;
#endif
    case ResourceType::EventSource:
        return Protocol::Page::ResourceType::EventSource;
    case ResourceType::Other:
        return Protocol::Page::ResourceType::Other;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Page::ResourceType::Other;
}

static Protocol::Network::Response::Source toProtocolResponseSource(ResourceResponse::Source source)
{
    switch (source) {
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::LegacyApplicationCachePlaceholder:
    case ResourceResponse::Source::Unknown:
        return Protocol::Network::Response::Source::Unknown;
    case ResourceResponse::Source::Network:
        return Protocol::Network::Response::Source::Network;
    case ResourceResponse::Source::MemoryCache:
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        return Protocol::Network::Response::Source::MemoryCache;
    case ResourceResponse::Source::DiskCache:
    case ResourceResponse::Source::DiskCacheAfterValidation:
        return Protocol::Network::Response::Source::DiskCache;
    case ResourceResponse::Source::ServiceWorker:
        return Protocol::Network::Response::Source::ServiceWorker;
    case ResourceResponse::Source::InspectorOverride:
        return Protocol::Network::Response::Source::InspectorOverride;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Network::Response::Source::Unknown;
}

// Raw monotonic seconds, not seconds elapsed on an InspectorEnvironment stopwatch: timestamps here
// come from several WebContent processes and have to share an epoch to land on one waterfall. See
// ResourceUtilities::buildObjectForTiming.
static double monotonicTimeToProtocolSeconds(MonotonicTime time)
{
    return time.secondsSinceEpoch().value();
}

static RefPtr<Protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse& response, std::optional<MonotonicTime> resourceLoadStartTime)
{
    if (response.isNull())
        return nullptr;

    auto responseObject = Protocol::Network::Response::create()
        .setUrl(response.url().string())
        .setStatus(response.httpStatusCode())
        .setStatusText(response.httpStatusText())
        .setHeaders(ResourceUtilities::buildObjectForHeaders(response.httpHeaderFields()))
        .setMimeType(response.mimeType())
        .setSource(toProtocolResponseSource(response.source()))
        .release();

    // Redirects and memory-cache hits have no start time, because they have no ResourceLoader and
    // that is what keeps the timings, so there is no phase breakdown to report for them.
    if (resourceLoadStartTime) {
        auto* metrics = response.deprecatedNetworkLoadMetricsOrNull();
        responseObject->setTiming(ResourceUtilities::buildObjectForTiming(metrics ? *metrics : NetworkLoadMetrics::emptyMetrics(), *resourceLoadStartTime, monotonicTimeToProtocolSeconds));
    }

    if (auto& certificateInfo = response.certificateInfo())
        responseObject->setSecurity(ResourceUtilities::buildObjectForSecurity(*certificateInfo));

    return responseObject;
}

ProxyingNetworkAgent::ProxyingNetworkAgent(WebKit::WebPageAgentContext& context)
    : InspectorAgentBase("Network"_s, context)
    , m_frontendDispatcher(makeUniqueRef<NetworkFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(NetworkBackendDispatcher::create(protect(context.backendDispatcher), this))
    , m_inspectedPage(context.inspectedPage)
{
}

ProxyingNetworkAgent::~ProxyingNetworkAgent()
{
    // Backstop in case Inspector teardown bypasses willDestroyFrontendAndBackend().
    removeAllRegisteredReceivers();
}

void ProxyingNetworkAgent::removeAllRegisteredReceivers()
{
    // Use the pinned WebProcessProxy refs rather than WebProcessProxy::processForIdentifier(),
    // which can return null when the process has been destructed but its receiver map entry
    // has not yet been torn down. The pin guarantees the process (and its receiver map) is
    // still alive here so removeMessageReceiver() can decrement m_messageReceiverMapCount.
    for (auto& [key, _] : std::exchange(m_instrumentedProcessPageCounts, { })) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->removeMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID);
    }
    m_pinnedInstrumentedProcesses.clear();
}

void ProxyingNetworkAgent::didCreateFrontendAndBackend()
{
    std::ignore = enable();
}

void ProxyingNetworkAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    std::ignore = disable();
}

void ProxyingNetworkAgent::enableInstrumentationForProcess(WebKit::WebProcessProxy& webProcess, WebCore::PageIdentifier pageID)
{
    auto key = std::make_pair(webProcess.coreProcessIdentifier(), pageID);
    auto result = m_instrumentedProcessPageCounts.add(key, 0);
    if (++result.iterator->value > 1)
        return;

    m_pinnedInstrumentedProcesses.ensure(webProcess.coreProcessIdentifier(), [&] {
        return Ref { webProcess };
    });
    webProcess.addMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID, *this);
    webProcess.send(Messages::WebInspectorBackend::EnableNetworkInstrumentation { }, pageID);

    // Catch a newly-instrumented process up to the latched network overrides.
    if (!m_extraRequestHeaders.isEmpty())
        webProcess.send(Messages::WebInspectorBackend::SetExtraHTTPHeaders { m_extraRequestHeaders }, pageID);
    if (m_resourceCachingDisabled)
        webProcess.send(Messages::WebInspectorBackend::SetResourceCachingDisabled { m_resourceCachingDisabled }, pageID);
}

void ProxyingNetworkAgent::disableInstrumentationForProcess(WebKit::WebProcessProxy& webProcess, WebCore::PageIdentifier pageID)
{
    auto processID = webProcess.coreProcessIdentifier();
    auto key = std::make_pair(processID, pageID);
    auto it = m_instrumentedProcessPageCounts.find(key);
    if (it == m_instrumentedProcessPageCounts.end())
        return;

    if (--it->value > 0)
        return;

    m_instrumentedProcessPageCounts.remove(it);
    webProcess.send(Messages::WebInspectorBackend::DisableNetworkInstrumentation { }, pageID);
    webProcess.removeMessageReceiver(Messages::ProxyingNetworkAgent::messageReceiverName(), pageID);

    // Drop the pin once this process has no remaining page registrations.
    bool processStillHasRegistrations = false;
    for (auto& entry : m_instrumentedProcessPageCounts) {
        if (entry.key.first == processID) {
            processStillHasRegistrations = true;
            break;
        }
    }
    if (!processStillHasRegistrations) {
        m_pinnedInstrumentedProcesses.remove(processID);

        // Its loads no longer report to us, so a paused one can't be resolved through this agent.
        auto isInProcess = [processID](auto& entry) {
            return entry.value.processIdentifier() == processID;
        };
        m_pendingInterceptRequests.removeIf(isInProcess);
        m_pendingInterceptResponses.removeIf(isInProcess);
    }
}

CommandResult<void> ProxyingNetworkAgent::enable()
{
    // FIXME: <https://webkit.org/b/308890> Only needed under Site Isolation; without it,
    // InspectorNetworkAgent in the single WebContent process handles network events.
    Ref inspectedPage = m_inspectedPage;
    Ref prefs = inspectedPage->preferences();
    if (!prefs->siteIsolationEnabled())
        return { };

    m_enabled = true;

    inspectedPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        Ref protectedWebProcess { webProcess };
        enableInstrumentationForProcess(protectedWebProcess, pageID);
    });

    return { };
}

CommandResult<void> ProxyingNetworkAgent::disable()
{
    // Web Inspector's interception patterns go away with the domain, and the loads it paused go on
    // unchanged. Other consumers keep theirs. Mirrors InspectorNetworkAgent::disable(). This comes
    // before the early return because the interception commands do not require Network.enable, so
    // patterns can be in place while the domain is not enabled.
    continuePendingInterceptions();
    m_interceptionEnabled = false;
    m_interceptPatterns.clear();
    updateInterceptPatterns();

    if (!m_enabled)
        return { };

    m_enabled = false;

    // Force-teardown: disable all processes unconditionally, bypassing the
    // refcount discipline in disableInstrumentationForProcess(). This is
    // correct because disable() is called when the Network domain is torn
    // down entirely -- no per-frame refcount preservation is needed.
    //
    // Iterate the registration map, not forEachWebContentProcess(): under
    // Site Isolation a process may have swapped out while still holding our
    // message receiver, in which case forEachWebContentProcess() would no
    // longer enumerate it. The pinned process refs (see m_pinnedInstrumentedProcesses)
    // keep the WebProcessProxy alive long enough for the send + remove below.
    for (auto& [key, _] : m_instrumentedProcessPageCounts) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->send(Messages::WebInspectorBackend::DisableNetworkInstrumentation { }, pageID);
    }
    removeAllRegisteredReceivers();

    // Reset latched Network overrides. This agent outlives frontend disconnect/reconnect, so a
    // future frontend must not inherit stale state; the WebProcess side is undone by the
    // DisableNetworkInstrumentation messages above. Mirrors InspectorNetworkAgent::disable().
    m_extraRequestHeaders = { };
    m_resourceCachingDisabled = false;
#if ENABLE(INSPECTOR_NETWORK_THROTTLING)
    protect(protect(m_inspectedPage)->websiteDataStore())->setEmulatedConditions(std::nullopt, 0_s);
#endif

    return { };
}

CommandResult<void> ProxyingNetworkAgent::setExtraHTTPHeaders(Ref<JSON::Object>&& headers)
{
    HTTPHeaderMap headerMap;
    for (auto& entry : headers.get()) {
        Ref value = entry.value;
        if (auto stringValue = value->asString(); !!stringValue)
            headerMap.set(entry.key, stringValue);
    }
    m_extraRequestHeaders = WTF::move(headerMap);

    protect(m_inspectedPage)->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebInspectorBackend::SetExtraHTTPHeaders { m_extraRequestHeaders }, pageID);
    });

    return { };
}

// An empty error string on a reply is the AsyncReplyError synthesized on connection loss (the
// target WebProcess is gone); report that explicitly instead of surfacing it to the frontend as a
// spurious success. A non-empty error is a genuine backend failure and is forwarded verbatim.
static String replyFailureString(const String& replyError)
{
    if (!replyError.isEmpty())
        return replyError;
    return "Target WebProcess for requestId is no longer available"_s;
}

// Resolve a requestId-routed command to the WebContent process that performed the load. The
// requestId encodes the owning process identifier (see IdentifierRegistry::protocolRequestId); on
// failure the returned string is the frontend-facing error. Shared by the requestId-routed
// commands (getResponseBody, getSerializedCertificate), which then issue their own async IPC to the
// resolved process.
static CommandResultOf<Ref<WebKit::WebProcessProxy>, PageIdentifier, ResourceLoaderIdentifier> resolveRequestProcess(WebKit::WebPageProxy& inspectedPage, const Protocol::Network::RequestId& requestId)
{
    auto parsed = IdentifierRegistry::parseProtocolRequestId(requestId);
    if (!parsed)
        return makeUnexpected("Invalid requestId format"_s);

    auto [processIdentifier, resourceID] = *parsed;

    RefPtr<WebKit::WebProcessProxy> targetProcess;
    std::optional<PageIdentifier> targetPageID;
    inspectedPage.forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.coreProcessIdentifier() == processIdentifier) {
            targetProcess = &webProcess;
            targetPageID = pageID;
        }
    });

    if (!targetProcess || !targetPageID)
        return makeUnexpected("WebProcess not found for requestId"_s);

    return { { targetProcess.releaseNonNull(), *targetPageID, resourceID } };
}

void ProxyingNetworkAgent::getResponseBody(const Protocol::Network::RequestId& requestId, Ref<GetResponseBodyCallback>&& callback)
{
    auto resolved = resolveRequestProcess(protect(m_inspectedPage), requestId);
    if (!resolved) {
        callback->sendFailure(resolved.error());
        return;
    }

    auto [targetProcess, targetPageID, resourceID] = WTF::move(resolved.value());
    targetProcess->sendWithAsyncReply(
        Messages::WebInspectorBackend::GetResponseBody { resourceID },
        [callback = WTF::move(callback)](std::expected<std::pair<String, bool>, String>&& result) mutable {
            if (result) {
                auto& [content, base64Encoded] = result.value();
                callback->sendSuccess(content, base64Encoded);
            } else
                callback->sendFailure(replyFailureString(result.error()));
        },
        targetPageID);
}

void ProxyingNetworkAgent::getSerializedCertificate(const Protocol::Network::RequestId& requestId, Ref<GetSerializedCertificateCallback>&& callback)
{
    auto resolved = resolveRequestProcess(protect(m_inspectedPage), requestId);
    if (!resolved) {
        callback->sendFailure(resolved.error());
        return;
    }

    auto [targetProcess, targetPageID, resourceID] = WTF::move(resolved.value());
    targetProcess->sendWithAsyncReply(
        Messages::WebInspectorBackend::GetSerializedCertificate { resourceID },
        [callback = WTF::move(callback)](std::expected<String, String>&& result) mutable {
            if (result)
                callback->sendSuccess(result.value());
            else
                callback->sendFailure(replyFailureString(result.error()));
        },
        targetPageID);
}

CommandResult<void> ProxyingNetworkAgent::setResourceCachingDisabled(bool disabled)
{
    m_resourceCachingDisabled = disabled;

    protect(m_inspectedPage)->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        webProcess.send(Messages::WebInspectorBackend::SetResourceCachingDisabled { disabled }, pageID);
    });

    return { };
}

CommandResult<void> ProxyingNetworkAgent::setClearResourceDataOnNavigate(bool)
{
    // FIXME: Forward to all WebContent processes.
    return { };
}

void ProxyingNetworkAgent::loadResource(const Protocol::Network::FrameId& frameId, const String& url, Ref<LoadResourceCallback>&& callback)
{
    // Routed by frame (frameId + URL), unlike getResponseBody which is routed by requestId: decode the
    // frame's hosting process from the frameId and forward the load to that process's WebInspectorBackend.
    auto parsed = IdentifierRegistry::parseProtocolFrameId(frameId);
    if (!parsed) {
        callback->sendFailure("Invalid frameId format"_s);
        return;
    }

    auto [processIdentifier, frameID] = *parsed;

    RefPtr<WebKit::WebProcessProxy> targetProcess;
    std::optional<PageIdentifier> targetPageID;

    protect(m_inspectedPage)->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        if (webProcess.coreProcessIdentifier() == processIdentifier) {
            targetProcess = &webProcess;
            targetPageID = pageID;
        }
    });

    if (!targetProcess || !targetPageID) {
        callback->sendFailure("WebProcess not found for frameId"_s);
        return;
    }

    targetProcess->sendWithAsyncReply(
        Messages::WebInspectorBackend::LoadResource { frameID, url },
        [callback = WTF::move(callback)](std::expected<std::tuple<String, String, int>, String>&& result) mutable {
            if (result) {
                auto& [content, mimeType, status] = result.value();
                callback->sendSuccess(content, mimeType, status);
            } else if (!result.error().isEmpty())
                callback->sendFailure(result.error());
            else {
                // Empty error string == AsyncReplyError synthesized on connection loss (target
                // WebProcess is gone), matching getResponseBody.
                callback->sendFailure("Target WebProcess for frameId is no longer available"_s);
            }
        },
        *targetPageID);
}

CommandResult<Ref<Protocol::Runtime::RemoteObject>> ProxyingNetworkAgent::resolveWebSocket(const Protocol::Network::RequestId&, const String&)
{
    return makeUnexpected("Not yet implemented"_s);
}

// Network interception under Site Isolation. Web Inspector is one consumer of the page's
// interception authority (WebPageProxy::setNetworkInterceptPatterns, owner Inspector); WebDriver
// BiDi is the other. A matching load pauses in the NetworkInterceptionSink of the WebContent
// process that started it, FrameNetworkAgentProxy reports the pause here, and the Network.intercept*
// commands resolve it in that process through WebPageProxy::resolveNetworkInterception. The
// requestId of a paused load names the process (see IdentifierRegistry::protocolRequestId), so
// the commands work for loads of every frame of the page, whichever process hosts it.

static NetworkInterceptionPhase toInterceptionPhase(Protocol::Network::NetworkStage stage)
{
    switch (stage) {
    case Protocol::Network::NetworkStage::Request:
        return NetworkInterceptionPhase::BeforeRequestSent;
    case Protocol::Network::NetworkStage::Response:
        return NetworkInterceptionPhase::ResponseStarted;
    }
    ASSERT_NOT_REACHED();
    return NetworkInterceptionPhase::ResponseStarted;
}

static InterceptPattern toInterceptPattern(const String& url, Protocol::Network::NetworkStage stage, const std::optional<bool>& caseSensitive, const std::optional<bool>& isRegex)
{
    return InterceptPattern {
        NetworkInterceptOwner::Inspector,
        toInterceptionPhase(stage),
        InspectorURLPattern { url, caseSensitive.value_or(true), isRegex.value_or(false) },
    };
}

static bool isSiteIsolationEnabled(WebKit::WebPageProxy* page)
{
    return page && protect(page->preferences())->siteIsolationEnabled();
}

void ProxyingNetworkAgent::updateInterceptPatterns(CompletionHandler<void()>&& completionHandler)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return completionHandler();

    // Without Site Isolation the WebContent process's own Network agent intercepts; the authority
    // must not hold Web Inspector's patterns twice.
    Vector<InterceptPattern> patterns;
    if (m_interceptionEnabled && isSiteIsolationEnabled(inspectedPage.get()))
        patterns = m_interceptPatterns;
    // The authority calls back once every process hosting a frame of the page has the patterns.
    inspectedPage->setNetworkInterceptPatterns(NetworkInterceptOwner::Inspector, WTF::move(patterns), WTF::move(completionHandler));
}

void ProxyingNetworkAgent::continuePendingInterceptions()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    auto pendingRequests = std::exchange(m_pendingInterceptRequests, { });
    auto pendingResponses = std::exchange(m_pendingInterceptResponses, { });
    if (!inspectedPage)
        return;

    for (auto& resourceID : pendingRequests.values())
        inspectedPage->resolveNetworkInterception(resourceID, NetworkInterceptOwner::Inspector, NetworkInterceptionPhase::BeforeRequestSent, { });
    for (auto& resourceID : pendingResponses.values())
        inspectedPage->resolveNetworkInterception(resourceID, NetworkInterceptOwner::Inspector, NetworkInterceptionPhase::ResponseStarted, { });
}

// Same errors as InspectorNetworkAgent, so the frontend and the tests see one behavior. The
// configuration commands reply only once every WebContent process hosting a frame of the page has
// the new patterns, so a load that starts after the reply sees them.

void ProxyingNetworkAgent::setInterceptionEnabled(bool enabled, Ref<SetInterceptionEnabledCallback>&& callback)
{
    // Unlike InspectorNetworkAgent, setting the current state again is not an error: under Site
    // Isolation the frontend enables interception on this target only once the first frame target
    // appears (NetworkManager.initializeTarget), possibly after a client already enabled it.
    if (m_interceptionEnabled == enabled) {
        callback->sendSuccess();
        return;
    }

    m_interceptionEnabled = enabled;
    updateInterceptPatterns([callback = WTF::move(callback)] {
        callback->sendSuccess();
    });

    if (!m_interceptionEnabled)
        continuePendingInterceptions();
}

void ProxyingNetworkAgent::addInterception(const String& url, Protocol::Network::NetworkStage stage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, Ref<AddInterceptionCallback>&& callback)
{
    if (!m_interceptPatterns.appendIfNotContains(toInterceptPattern(url, stage, caseSensitive, isRegex))) {
        callback->sendFailure("Intercept for given url, given isRegex, and given stage already exists"_s);
        return;
    }

    updateInterceptPatterns([callback = WTF::move(callback)] {
        callback->sendSuccess();
    });
}

void ProxyingNetworkAgent::removeInterception(const String& url, Protocol::Network::NetworkStage stage, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, Ref<RemoveInterceptionCallback>&& callback)
{
    if (!m_interceptPatterns.removeAll(toInterceptPattern(url, stage, caseSensitive, isRegex))) {
        callback->sendFailure("Missing intercept for given url, given isRegex, and given stage"_s);
        return;
    }

    updateInterceptPatterns([callback = WTF::move(callback)] {
        callback->sendSuccess();
    });
}

static ASCIILiteral missingPendingInterceptError(NetworkInterceptionPhase phase)
{
    return phase == NetworkInterceptionPhase::BeforeRequestSent ? "Missing pending intercept request for given requestId"_s : "Missing pending intercept response for given requestId"_s;
}

CommandResult<void> ProxyingNetworkAgent::resolveInterception(const Protocol::Network::RequestId& requestId, NetworkInterceptionPhase phase, NetworkInterceptionResolution&& resolution)
{
    // Like InspectorNetworkAgent, a paused response can only go on or be replaced, never failed.
    if (phase == NetworkInterceptionPhase::ResponseStarted && resolution.action == NetworkInterceptionAction::Fail)
        return makeUnexpected("Unable to fail an intercepted response"_s);

    auto& pendingInterceptions = phase == NetworkInterceptionPhase::BeforeRequestSent ? m_pendingInterceptRequests : m_pendingInterceptResponses;
    auto iterator = pendingInterceptions.find(requestId);
    if (iterator == pendingInterceptions.end())
        return makeUnexpected(missingPendingInterceptError(phase));
    auto resourceID = iterator->value;
    pendingInterceptions.remove(iterator);

    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return makeUnexpected("Inspected page is gone"_s);

    // The process that paused the load applies the resolution. A load that went away in the
    // meantime ignores it, like the single-process agent ignores a load in a terminal state.
    inspectedPage->resolveNetworkInterception(resourceID, NetworkInterceptOwner::Inspector, phase, WTF::move(resolution));
    return { };
}

static Vector<std::pair<String, String>> headerPairs(const JSON::Object& headers)
{
    Vector<std::pair<String, String>> pairs;
    for (auto& [name, value] : headers) {
        auto headerValue = protect(value)->asString();
        if (!!headerValue)
            pairs.append({ name, headerValue });
    }
    return pairs;
}

static std::optional<Vector<uint8_t>> decodeContent(const String& content, bool base64Encoded)
{
    if (base64Encoded)
        return base64Decode(content);
    auto utf8 = content.utf8();
    return Vector<uint8_t> { byteCast<uint8_t>(utf8.span()) };
}

CommandResult<void> ProxyingNetworkAgent::interceptContinue(const Protocol::Network::RequestId& requestId, Protocol::Network::NetworkStage networkStage)
{
    return resolveInterception(requestId, toInterceptionPhase(networkStage), { });
}

CommandResult<void> ProxyingNetworkAgent::interceptWithRequest(const Protocol::Network::RequestId& requestId, const String& url, const String& method, RefPtr<JSON::Object>&& headers, const String& postData)
{
    if (!m_pendingInterceptRequests.contains(requestId))
        return makeUnexpected(missingPendingInterceptError(NetworkInterceptionPhase::BeforeRequestSent));

    NetworkInterceptionResolution resolution;
    if (!!url)
        resolution.url = URL({ }, url);
    if (!!method)
        resolution.method = method;
    if (headers)
        resolution.headers = headerPairs(*headers);
    if (!!postData) {
        auto body = base64Decode(postData);
        if (!body)
            return makeUnexpected("Unable to decode given postData"_s);
        resolution.body = SharedBuffer::create(WTF::move(*body));
    }

    return resolveInterception(requestId, NetworkInterceptionPhase::BeforeRequestSent, WTF::move(resolution));
}

CommandResult<void> ProxyingNetworkAgent::interceptWithResponse(const Protocol::Network::RequestId& requestId, const String& content, bool base64Encoded, const String& mimeType, std::optional<int>&& status, const String& statusText, RefPtr<JSON::Object>&& headers)
{
    if (!m_pendingInterceptResponses.contains(requestId))
        return makeUnexpected(missingPendingInterceptError(NetworkInterceptionPhase::ResponseStarted));

    auto body = decodeContent(content, base64Encoded);
    if (!body)
        return makeUnexpected("Unable to decode given content"_s);

    NetworkInterceptionResolution resolution;
    resolution.action = NetworkInterceptionAction::Fulfill;
    resolution.body = SharedBuffer::create(WTF::move(*body));
    resolution.statusCode = status;
    if (!!statusText)
        resolution.reasonPhrase = statusText;
    if (!!mimeType)
        resolution.mimeType = mimeType;
    if (headers)
        resolution.headers = headerPairs(*headers);

    return resolveInterception(requestId, NetworkInterceptionPhase::ResponseStarted, WTF::move(resolution));
}

CommandResult<void> ProxyingNetworkAgent::interceptRequestWithResponse(const Protocol::Network::RequestId& requestId, const String& content, bool base64Encoded, const String& mimeType, int status, const String& statusText, Ref<JSON::Object>&& headers)
{
    if (!m_pendingInterceptRequests.contains(requestId))
        return makeUnexpected(missingPendingInterceptError(NetworkInterceptionPhase::BeforeRequestSent));

    auto body = decodeContent(content, base64Encoded);
    if (!body)
        return makeUnexpected("Unable to decode given content"_s);

    NetworkInterceptionResolution resolution;
    resolution.action = NetworkInterceptionAction::Fulfill;
    resolution.body = SharedBuffer::create(WTF::move(*body));
    resolution.statusCode = status;
    resolution.reasonPhrase = statusText;
    resolution.mimeType = mimeType;
    resolution.headers = headerPairs(headers);

    return resolveInterception(requestId, NetworkInterceptionPhase::BeforeRequestSent, WTF::move(resolution));
}

static NetworkInterceptionErrorType toInterceptionErrorType(Protocol::Network::ResourceErrorType errorType)
{
    switch (errorType) {
    case Protocol::Network::ResourceErrorType::General:
        return NetworkInterceptionErrorType::General;
    case Protocol::Network::ResourceErrorType::AccessControl:
        return NetworkInterceptionErrorType::AccessControl;
    case Protocol::Network::ResourceErrorType::Cancellation:
        return NetworkInterceptionErrorType::Cancellation;
    case Protocol::Network::ResourceErrorType::Timeout:
        return NetworkInterceptionErrorType::Timeout;
    }
    ASSERT_NOT_REACHED();
    return NetworkInterceptionErrorType::General;
}

CommandResult<void> ProxyingNetworkAgent::interceptRequestWithError(const Protocol::Network::RequestId& requestId, Protocol::Network::ResourceErrorType errorType)
{
    NetworkInterceptionResolution resolution;
    resolution.action = NetworkInterceptionAction::Fail;
    resolution.errorType = toInterceptionErrorType(errorType);
    return resolveInterception(requestId, NetworkInterceptionPhase::BeforeRequestSent, WTF::move(resolution));
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

CommandResult<void> ProxyingNetworkAgent::setEmulatedConditions(std::optional<int>&& bandwidth, std::optional<int>&& latency)
{
    if (bandwidth && *bandwidth < 0)
        return makeUnexpected("bandwidth cannot be negative"_s);

    if (latency && *latency < 0)
        return makeUnexpected("latency cannot be negative"_s);

    std::optional<uint64_t> bandwidthBytesPerSecond;
    if (bandwidth)
        bandwidthBytesPerSecond = *bandwidth;

    protect(protect(m_inspectedPage)->websiteDataStore())->setEmulatedConditions(bandwidthBytesPerSecond, Seconds::fromMilliseconds(latency.value_or(0)));
    return { };
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

// IPC message handlers from WebProcess FrameNetworkAgentProxy.

void ProxyingNetworkAgent::requestWillBeSent(ResourceID resourceID, FrameID frameID, const String& loaderId, const String& targetID, const String& documentURL, const ResourceRequest& request, const RequestExtras& requestExtras, std::optional<ResourceResponse>&& redirectResponse, ResourceType resourceType, double timestamp, double walltime, InitiatorData&& initiator)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto requestObject = ResourceUtilities::buildObjectForResourceRequest(request, requestExtras);
    auto initiatorObject = ResourceUtilities::buildInitiatorObject(initiator);

    RefPtr<Protocol::Network::Response> redirectResponseObject;
    if (redirectResponse)
        redirectResponseObject = buildObjectForResourceResponse(*redirectResponse, std::nullopt);

    m_frontendDispatcher->requestWillBeSent(requestId, frameIdString, loaderId, documentURL, WTF::move(requestObject), timestamp, walltime, WTF::move(initiatorObject), WTF::move(redirectResponseObject), toProtocolResourceType(resourceType), targetID);
}

void ProxyingNetworkAgent::responseReceived(ResourceID resourceID, FrameID frameID, const String& loaderId, const ResourceResponse& response, ResourceType resourceType, double timestamp, std::optional<MonotonicTime> resourceLoadStartTime)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto responseObject = buildObjectForResourceResponse(response, resourceLoadStartTime);

    if (responseObject)
        m_frontendDispatcher->responseReceived(requestId, frameIdString, loaderId, timestamp, toProtocolResourceType(resourceType), responseObject.releaseNonNull());
}

void ProxyingNetworkAgent::dataReceived(ResourceID resourceID, int dataLength, int encodedDataLength, double timestamp)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    m_frontendDispatcher->dataReceived(requestId, timestamp, dataLength, encodedDataLength);
}

void ProxyingNetworkAgent::loadingFinished(ResourceID resourceID, double timestamp, const String& sourceMapURL, NetworkLoadMetrics&& metrics)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    m_frontendDispatcher->loadingFinished(requestId, timestamp, sourceMapURL, ResourceUtilities::buildObjectForMetrics(metrics));
}

void ProxyingNetworkAgent::loadingFailed(ResourceID resourceID, double timestamp, const String& errorText, bool canceled)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    // A paused load that fails (for example, its frame went away) has nothing left to resolve.
    m_pendingInterceptRequests.remove(requestId);
    m_pendingInterceptResponses.remove(requestId);
    m_frontendDispatcher->loadingFailed(requestId, timestamp, errorText, canceled);
}

void ProxyingNetworkAgent::requestServedFromMemoryCache(ResourceID resourceID, FrameID frameID, const String& loaderId, const String& documentURL, const ResourceResponse& response, ResourceType resourceType, const String& sourceMapURL, uint64_t bodySize, double timestamp, InitiatorData&& initiator)
{
    if (!m_enabled)
        return;

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());
    auto frameIdString = IdentifierRegistry::protocolFrameId(frameID, resourceID.processIdentifier());
    auto cachedResourceObject = Protocol::Network::CachedResource::create()
        .setUrl(response.url().string())
        .setType(toProtocolResourceType(resourceType))
        .setBodySize(bodySize)
        .release();

    // A memory-cached resource never went to the network, so there is no load timing to report.
    if (auto responseObject = buildObjectForResourceResponse(response, std::nullopt))
        cachedResourceObject->setResponse(responseObject.releaseNonNull());

    if (!sourceMapURL.isEmpty())
        cachedResourceObject->setSourceMapURL(sourceMapURL);

    m_frontendDispatcher->requestServedFromMemoryCache(requestId, frameIdString, loaderId, documentURL, timestamp, ResourceUtilities::buildInitiatorObject(initiator), WTF::move(cachedResourceObject));
}

void ProxyingNetworkAgent::requestIntercepted(IPC::Connection& connection, ResourceID resourceID, const ResourceRequest& request)
{
    // The resolution is routed to the process named by the resource id, so only the process that paused the load may name it.
    MESSAGE_CHECK_BASE(resourceID.processIdentifier() == WebKit::WebProcessProxy::fromConnection(connection)->coreProcessIdentifier(), connection);

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());

    // Nobody can resolve the pause once Web Inspector stopped intercepting; let the load go on.
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!m_enabled || !m_interceptionEnabled) {
        if (inspectedPage)
            inspectedPage->resolveNetworkInterception(resourceID, NetworkInterceptOwner::Inspector, NetworkInterceptionPhase::BeforeRequestSent, { });
        return;
    }

    m_pendingInterceptRequests.set(requestId, resourceID);
    m_frontendDispatcher->requestIntercepted(requestId, ResourceUtilities::buildObjectForResourceRequest(request, { }));
}

void ProxyingNetworkAgent::responseIntercepted(IPC::Connection& connection, ResourceID resourceID, const ResourceResponse& response)
{
    MESSAGE_CHECK_BASE(resourceID.processIdentifier() == WebKit::WebProcessProxy::fromConnection(connection)->coreProcessIdentifier(), connection);

    auto requestId = IdentifierRegistry::protocolRequestId(resourceID.processIdentifier(), resourceID.object());

    RefPtr inspectedPage = m_inspectedPage.get();
    auto responseObject = buildObjectForResourceResponse(response, std::nullopt);
    if (!m_enabled || !m_interceptionEnabled || !responseObject) {
        if (inspectedPage)
            inspectedPage->resolveNetworkInterception(resourceID, NetworkInterceptOwner::Inspector, NetworkInterceptionPhase::ResponseStarted, { });
        return;
    }

    m_pendingInterceptResponses.set(requestId, resourceID);
    m_frontendDispatcher->responseIntercepted(requestId, responseObject.releaseNonNull());
}

} // namespace Inspector
