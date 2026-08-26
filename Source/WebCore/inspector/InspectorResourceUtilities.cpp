/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorResourceUtilities.h"

#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentResourceLoader.h"
#include "FetchOptions.h"
#include "FrameLoader.h"
#include "HTTPHeaderMap.h"
#include "InspectorResourceType.h"
#include "InspectorThreadableLoaderClient.h"
#include "InstrumentingAgents.h"
#include "JSExecState.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "NetworkLoadMetrics.h"
#include "Page.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "ScriptExecutionContext.h"
#include "ScriptableDocumentParser.h"
#include "SharedBuffer.h"
#include "ThreadableLoader.h"
#include <JavaScriptCore/AsyncStackTrace.h>
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <limits>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace Inspector {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(InitiatorStackTrace);

namespace ResourceUtilities {

using namespace WebCore;

Inspector::Protocol::Page::ResourceType resourceTypeToProtocol(Inspector::ResourceType resourceType)
{
    switch (resourceType) {
    case ResourceType::Document:
        return Inspector::Protocol::Page::ResourceType::Document;
    case ResourceType::Image:
        return Inspector::Protocol::Page::ResourceType::Image;
    case ResourceType::Font:
        return Inspector::Protocol::Page::ResourceType::Font;
    case ResourceType::StyleSheet:
        return Inspector::Protocol::Page::ResourceType::StyleSheet;
    case ResourceType::Script:
        return Inspector::Protocol::Page::ResourceType::Script;
    case ResourceType::XHR:
        return Inspector::Protocol::Page::ResourceType::XHR;
    case ResourceType::Fetch:
        return Inspector::Protocol::Page::ResourceType::Fetch;
    case ResourceType::Ping:
        return Inspector::Protocol::Page::ResourceType::Ping;
    case ResourceType::Beacon:
        return Inspector::Protocol::Page::ResourceType::Beacon;
    case ResourceType::WebSocket:
        return Inspector::Protocol::Page::ResourceType::WebSocket;
    case ResourceType::EventSource:
        return Inspector::Protocol::Page::ResourceType::EventSource;
    case ResourceType::Other:
        return Inspector::Protocol::Page::ResourceType::Other;
#if ENABLE(APPLICATION_MANIFEST)
    case ResourceType::ApplicationManifest:
        break;
#endif
    }
    return Inspector::Protocol::Page::ResourceType::Other;
}

[[nodiscard]] static bool decodeBuffer(std::span<const uint8_t> buffer, const String& textEncodingName, String* result)
{
    if (buffer.data()) {
        PAL::TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = PAL::WindowsLatin1Encoding();
        *result = encoding.decode(buffer);
        return true;
    }
    return false;
}

static bool dataContent(std::span<const uint8_t> data, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64EncodeToString(data);
        return true;
    }

    return decodeBuffer(data, textEncodingName, result);
}

bool sharedBufferContent(RefPtr<FragmentedSharedBuffer>&& buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->makeContiguous()->span() : std::span<const uint8_t> { }, textEncodingName, withBase64Encode, result);
}

Vector<CachedResource*> cachedResourcesForFrame(LocalFrame* frame)
{
    Vector<CachedResource*> result;

    for (auto& cachedResourceHandle : protect(frame->document())->cachedResourceLoader().allCachedResources().values()) {
        RefPtr cachedResource = cachedResourceHandle;
        if (cachedResource->resourceRequest().hiddenFromInspector())
            continue;

        switch (cachedResource->type()) {
        case CachedResource::Type::ImageResource:
            // Skip images that were not auto loaded (images disabled in the user agent).
        case CachedResource::Type::SVGFontResource:
        case CachedResource::Type::FontResource:
            // Skip fonts that were referenced in CSS but never used/downloaded.
            if (cachedResource->stillNeedsLoad())
                continue;
            break;
        default:
            // All other CachedResource types download immediately.
            break;
        }

        result.append(cachedResource);
    }

    return result;
}

bool mainResourceContent(LocalFrame* frame, bool withBase64Encode, String* result)
{
    RefPtr<FragmentedSharedBuffer> buffer = frame->loader().documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    return dataContent(buffer->makeContiguous()->span(), protect(frame->document())->encoding(), withBase64Encode, result);
}

void resourceContent(Inspector::Protocol::ErrorString& errorString, LocalFrame* frame, const URL& url, String* result, bool* base64Encoded)
{
    RefPtr<DocumentLoader> loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return;

    RefPtr<FragmentedSharedBuffer> buffer;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(url, loader->url())) {
        *base64Encoded = false;
        success = mainResourceContent(frame, *base64Encoded, result);
    }

    if (!success) {
        if (RefPtr resource = cachedResource(frame, url))
            success = cachedResourceContent(*resource, result, base64Encoded);
    }

    if (!success)
        errorString = "Missing resource for given url"_s;
}

Ref<JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>> buildResourceObjectsForFrame(LocalFrame& frame)
{
    auto resources = JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>::create();
    for (auto& resource : buildResourceDataForFrame(frame))
        resources->addItem(buildResourceObject(resource));
    return resources;
}

Vector<Inspector::FrameResource> buildResourceDataForFrame(LocalFrame& frame)
{
    Vector<Inspector::FrameResource> resources;
    for (RefPtr cachedResource : cachedResourcesForFrame(&frame)) {
        Inspector::FrameResource resource;
        resource.url = cachedResource->url().string();
        resource.type = inspectorResourceType(*cachedResource);
        resource.mimeType = cachedResource->response().mimeType();
        if (cachedResource->wasCanceled())
            resource.canceled = true;
        else if (cachedResource->status() == CachedResource::LoadError || cachedResource->status() == CachedResource::DecodeError)
            resource.failed = true;
        resource.sourceMapURL = sourceMapURLForResource(cachedResource.get());
        resource.targetId = cachedResource->resourceRequest().initiatorIdentifier();
        resources.append(WTF::move(resource));
    }
    return resources;
}

Ref<Inspector::Protocol::Page::FrameResource> buildResourceObject(const Inspector::FrameResource& resource)
{
    auto resourceObject = Inspector::Protocol::Page::FrameResource::create()
        .setUrl(resource.url)
        .setType(resourceTypeToProtocol(resource.type))
        .setMimeType(resource.mimeType)
        .release();
    if (resource.canceled)
        resourceObject->setCanceled(true);
    else if (resource.failed)
        resourceObject->setFailed(true);
    if (!resource.sourceMapURL.isEmpty())
        resourceObject->setSourceMapURL(resource.sourceMapURL);
    if (!resource.targetId.isEmpty())
        resourceObject->setTargetId(resource.targetId);
    return resourceObject;
}

String sourceMapURLForResource(CachedResource* cachedResource)
{
    if (!cachedResource)
        return String();

    // Scripts are handled in a separate path.
    if (cachedResource->type() != CachedResource::Type::CSSStyleSheet)
        return String();

    String sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::SourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::XSourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    String content;
    bool base64Encoded;
    if (cachedResourceContent(*cachedResource, &content, &base64Encoded) && !base64Encoded)
        return ContentSearchUtilities::findStylesheetSourceMapURL(content);

    return String();
}

RefPtr<CachedResource> cachedResource(const LocalFrame* frame, const URL& url)
{
    if (url.isNull())
        return nullptr;

    RefPtr cachedResource = protect(frame->document())->cachedResourceLoader().cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(url));
    if (!cachedResource) {
        ResourceRequest request(URL { url });
        if (RefPtr document = frame->document()) {
            request.setShouldBlockThirdPartyStorage(document->shouldBlockThirdPartyStorage());
            request.setFirstPartyForCookies(document->firstPartyForCookies());
        }
        cachedResource = MemoryCache::singleton().resourceForRequest(request, frame->page()->sessionID());
    }

    return cachedResource;
}

Inspector::ResourceType inspectorResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return ResourceType::Image;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
        return ResourceType::Font;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::CSSStyleSheet:
        return ResourceType::StyleSheet;
    case CachedResource::Type::JSON: // FIXME: Add ResourceType::JSON.
    case CachedResource::Type::Text:
    case CachedResource::Type::Script:
        return ResourceType::Script;
    case CachedResource::Type::MainResource:
        return ResourceType::Document;
    case CachedResource::Type::Beacon:
        return ResourceType::Beacon;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return ResourceType::ApplicationManifest;
#endif
    case CachedResource::Type::Ping:
        return ResourceType::Ping;
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    default:
        return ResourceType::Other;
    }
}

ResourceType inspectorResourceType(const CachedResource& cachedResource)
{
    if (cachedResource.type() == CachedResource::Type::MainResource && MIMETypeRegistry::isSupportedImageMIMEType(cachedResource.mimeType()))
        return ResourceType::Image;

    if (cachedResource.type() == CachedResource::Type::RawResource) {
        switch (cachedResource.resourceRequest().requester()) {
        case ResourceRequestRequester::Fetch:
            return ResourceType::Fetch;
        case ResourceRequestRequester::Main:
            return ResourceType::Document;
        case ResourceRequestRequester::EventSource:
            return ResourceType::EventSource;
        default:
            return ResourceType::XHR;
        }
    }

    return inspectorResourceType(cachedResource.type());
}

Inspector::Protocol::Page::ResourceType cachedResourceTypeToProtocol(const CachedResource& cachedResource)
{
    return resourceTypeToProtocol(inspectorResourceType(cachedResource));
}

LocalFrame* findFrameWithSecurityOrigin(Page& page, const String& originRawString)
{
    // FIXME: this frame tree traversal needs to be redesigned for Site Isolation.
    for (SUPPRESS_UNCOUNTED_LOCAL auto* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        SUPPRESS_UNCOUNTED_LOCAL auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (protect(localFrame->document())->securityOrigin().toRawString() == originRawString)
            return localFrame;
    }
    return nullptr;
}

DocumentLoader* assertDocumentLoader(Inspector::Protocol::ErrorString& errorString, LocalFrame* frame)
{
    FrameLoader& frameLoader = frame->loader();
    SUPPRESS_UNCOUNTED_LOCAL auto* documentLoader = frameLoader.documentLoader();
    if (!documentLoader)
        errorString = "Missing document loader for given frame"_s;
    return documentLoader;
}

bool shouldTreatAsText(const String& mimeType)
{
    return startsWithLettersIgnoringASCIICase(mimeType, "text/"_s)
        || MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType)
        || MIMETypeRegistry::isSupportedJSONMIMEType(mimeType)
        || MIMETypeRegistry::isXMLMIMEType(mimeType)
        || MIMETypeRegistry::isTextMediaPlaylistMIMEType(mimeType);
}

Ref<TextResourceDecoder> createTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain"_s, textEncodingName);

    if (MIMETypeRegistry::isTextMIMEType(mimeType))
        return TextResourceDecoder::create(mimeType, "UTF-8"_s);

    if (MIMETypeRegistry::isXMLMIMEType(mimeType)) {
        auto decoder = TextResourceDecoder::create("application/xml"_s);
        decoder->useLenientXMLDecoding();
        return decoder;
    }

    return TextResourceDecoder::create("text/plain"_s, "UTF-8"_s);
}

std::optional<String> textContentForCachedResource(CachedResource& cachedResource)
{
    if (!shouldTreatAsText(cachedResource.mimeType()))
        return std::nullopt;

    String result;
    bool base64Encoded;
    if (cachedResourceContent(cachedResource, &result, &base64Encoded)) {
        ASSERT(!base64Encoded);
        return result;
    }

    return std::nullopt;
}

bool cachedResourceContent(CachedResource& resource, String* result, bool* base64Encoded)
{
    ASSERT(result);
    ASSERT(base64Encoded);

    if (!resource.encodedSize()) {
        *base64Encoded = false;
        *result = String();
        return true;
    }

    switch (resource.type()) {
    case CachedResource::Type::CSSStyleSheet:
        *base64Encoded = false;
        *result = downcast<CachedCSSStyleSheet>(resource).sheetText();
        // The above can return a null String if the MIME type is invalid.
        return !result->isNull();
    case CachedResource::Type::JSON:
    case CachedResource::Type::Text:
    case CachedResource::Type::Script:
        *base64Encoded = false;
        *result = downcast<CachedScript>(resource).script().toString();
        return true;
    default:
        RefPtr buffer = resource.resourceBuffer();
        if (!buffer)
            return false;

        if (shouldTreatAsText(resource.mimeType())) {
            auto decoder = createTextDecoder(resource.mimeType(), resource.response().textEncodingName());
            *base64Encoded = false;
            *result = decoder->decodeAndFlush(buffer->makeContiguous()->span());
            return true;
        }

        *base64Encoded = true;
        *result = base64EncodeToString(buffer->makeContiguous()->span());
        return true;
    }
}

void loadResource(ScriptExecutionContext& context, const String& urlString, LoadResourceCompletionHandler&& completionHandler)
{
    // Backs Network.loadResource: load a URL in a document's context on behalf of the inspector,
    // bypassing cross-origin checks (e.g. to fetch a source map).
    URL url = context.encodingParseURL(urlString);
    ResourceRequest request(WTF::move(url));
    request.setHTTPMethod("GET"_s);
    request.setHiddenFromInspector(true);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks; // So InspectorNetworkAgent's willSendRequest/loadingFinished hooks still fire for this hidden request, letting it track and untrack it.
    options.defersLoadingPolicy = DefersLoadingPolicy::DisallowDefersLoading; // So the request is never deferred.
    options.mode = FetchOptions::Mode::NoCors;
    options.credentials = FetchOptions::Credentials::SameOrigin;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicyEnforcement::DoNotEnforce;

    Ref client = InspectorThreadableLoaderClient::create(WTF::move(completionHandler));
    RefPtr loader = ThreadableLoader::create(context, client.get(), WTF::move(request), options);
    if (!loader) {
        client->failWithMessage("Could not load requested resource."_s);
        return;
    }

    // If the load already finished synchronously the client has disposed itself; only retain the
    // loader while the load is still in flight.
    if (client->isActive())
        client->setLoader(WTF::move(loader));
}

Ref<Inspector::Protocol::Network::Headers> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    auto headersValue = Inspector::Protocol::Network::Headers::create().release();
    auto headersObject = headersValue->asObject();
    for (const auto& header : headers)
        headersObject->setString(header.key, header.value);
    return headersValue;
}

static Inspector::Protocol::Network::Metrics::Priority NODELETE toProtocol(NetworkLoadPriority priority)
{
    switch (priority) {
    case NetworkLoadPriority::Low:
        return Inspector::Protocol::Network::Metrics::Priority::Low;
    case NetworkLoadPriority::Medium:
        return Inspector::Protocol::Network::Metrics::Priority::Medium;
    case NetworkLoadPriority::High:
        return Inspector::Protocol::Network::Metrics::Priority::High;
    case NetworkLoadPriority::Unknown:
        break;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Metrics::Priority::Medium;
}

Ref<Inspector::Protocol::Network::Metrics> buildObjectForMetrics(const NetworkLoadMetrics& networkLoadMetrics)
{
    auto metrics = Inspector::Protocol::Network::Metrics::create().release();

    if (!networkLoadMetrics.protocol.isNull())
        metrics->setProtocol(networkLoadMetrics.protocol);

    // The additional metrics are only captured while an inspector is attached
    // (InspectorInstrumentation::firstFrontendCreated enables it in the NetworkProcess).
    if (auto* additionalMetrics = networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector.get()) {
        if (additionalMetrics->priority != NetworkLoadPriority::Unknown)
            metrics->setPriority(toProtocol(additionalMetrics->priority));
        if (!additionalMetrics->remoteAddress.isNull())
            metrics->setRemoteAddress(additionalMetrics->remoteAddress);
        if (!additionalMetrics->connectionIdentifier.isNull())
            metrics->setConnectionIdentifier(additionalMetrics->connectionIdentifier);
        if (!additionalMetrics->requestHeaders.isEmpty())
            metrics->setRequestHeaders(buildObjectForHeaders(additionalMetrics->requestHeaders));
        if (additionalMetrics->requestHeaderBytesSent != std::numeric_limits<uint64_t>::max())
            metrics->setRequestHeaderBytesSent(additionalMetrics->requestHeaderBytesSent);
        if (additionalMetrics->requestBodyBytesSent != std::numeric_limits<uint64_t>::max())
            metrics->setRequestBodyBytesSent(additionalMetrics->requestBodyBytesSent);
        if (additionalMetrics->responseHeaderBytesReceived != std::numeric_limits<uint64_t>::max())
            metrics->setResponseHeaderBytesReceived(additionalMetrics->responseHeaderBytesReceived);
        metrics->setIsProxyConnection(additionalMetrics->isProxyConnection);
    }

    if (networkLoadMetrics.responseBodyBytesReceived != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyBytesReceived(networkLoadMetrics.responseBodyBytesReceived);
    if (networkLoadMetrics.responseBodyDecodedSize != std::numeric_limits<uint64_t>::max())
        metrics->setResponseBodyDecodedSize(networkLoadMetrics.responseBodyDecodedSize);

    auto connectionPayload = Inspector::Protocol::Security::Connection::create().release();

    if (auto* additionalMetrics = networkLoadMetrics.additionalNetworkLoadMetricsForWebInspector.get()) {
        if (!additionalMetrics->tlsProtocol.isEmpty())
            connectionPayload->setProtocol(additionalMetrics->tlsProtocol);
        if (!additionalMetrics->tlsCipher.isEmpty())
            connectionPayload->setCipher(additionalMetrics->tlsCipher);
    }

    metrics->setSecurityConnection(WTF::move(connectionPayload));

    return metrics;
}

Ref<Inspector::Protocol::Network::ResourceTiming> buildObjectForTiming(const NetworkLoadMetrics& timing, MonotonicTime loadStartTime, NOESCAPE const Function<double(MonotonicTime)>& monotonicToProtocolSeconds)
{
    auto millisecondsSinceFetchStart = [&](const MonotonicTime& time) {
        if (!time)
            return 0.0;
        return (time - timing.fetchStart).milliseconds();
    };

    return Inspector::Protocol::Network::ResourceTiming::create()
        .setStartTime(monotonicToProtocolSeconds(loadStartTime))
        .setRedirectStart(monotonicToProtocolSeconds(timing.redirectStart))
        .setRedirectEnd(monotonicToProtocolSeconds(timing.fetchStart))
        .setFetchStart(monotonicToProtocolSeconds(timing.fetchStart))
        .setDomainLookupStart(millisecondsSinceFetchStart(timing.domainLookupStart))
        .setDomainLookupEnd(millisecondsSinceFetchStart(timing.domainLookupEnd))
        .setConnectStart(millisecondsSinceFetchStart(timing.connectStart))
        .setConnectEnd(millisecondsSinceFetchStart(timing.connectEnd))
        .setSecureConnectionStart(millisecondsSinceFetchStart(timing.secureConnectionStart))
        .setRequestStart(millisecondsSinceFetchStart(timing.requestStart))
        .setResponseStart(millisecondsSinceFetchStart(timing.responseStart))
        .setResponseEnd(millisecondsSinceFetchStart(timing.responseEnd))
        .release();
}

static Vector<InitiatorCallFrame> gatherCallFrames(const ScriptCallStack& callStack)
{
    Vector<InitiatorCallFrame> callFrames;
    callFrames.reserveInitialCapacity(callStack.size());
    for (size_t i = 0; i < callStack.size(); ++i) {
        auto& frame = callStack.at(i);
        callFrames.append({ frame.functionName(), frame.sourceURL(), frame.sourceID(), frame.lineNumber(), frame.columnNumber() });
    }
    return callFrames;
}

// Mirror of AsyncStackTrace::buildInspectorObject, producing the plain, serializable form of the
// async parent chain. Returns null when every level is boundary-only, matching that method.
static std::unique_ptr<InitiatorStackTrace> gatherAsyncStackTrace(const AsyncStackTrace* asyncStackTrace)
{
    std::unique_ptr<InitiatorStackTrace> head;
    InitiatorStackTrace* tail = nullptr;

    for (auto* level = asyncStackTrace; level; level = level->parentStackTrace().get()) {
        bool truncated = level->truncated();
        bool topCallFrameIsBoundary = level->topCallFrameIsBoundary();

        // Skip async stack traces that only contain the boundary frame.
        if (topCallFrameIsBoundary && !truncated && level->size() == 1)
            continue;

        auto node = makeUnique<InitiatorStackTrace>();
        node->truncated = truncated;
        node->topCallFrameIsBoundary = topCallFrameIsBoundary;
        node->callFrames.reserveInitialCapacity(level->size());
        for (size_t i = 0; i < level->size(); ++i) {
            auto& frame = level->at(i);
            node->callFrames.append({ frame.functionName(), frame.sourceURL(), frame.sourceID(), frame.lineNumber(), frame.columnNumber() });
        }

        auto* appended = node.get();
        if (tail)
            tail->parentStackTrace = WTF::move(node);
        else
            head = WTF::move(node);
        tail = appended;
    }

    return head;
}

InitiatorData gatherInitiatorData(Document* document, const ResourceRequest* resourceRequest, const InstrumentingAgents& instrumentingAgents)
{
    InitiatorData data;

    // FIXME: Worker support. The JS stack below can only be read on the main thread, so a load
    // started from a worker is reported as unattributed.
    if (!isMainThread())
        return data;

    Ref<ScriptCallStack> stackTrace = createScriptCallStack(JSExecState::currentState());
    if (stackTrace->size() > 0) {
        data.type = InitiatorType::Script;
        data.stackTrace = makeUnique<InitiatorStackTrace>();
        data.stackTrace->truncated = stackTrace->truncated();
        data.stackTrace->callFrames = gatherCallFrames(stackTrace);
        // topCallFrameIsBoundary stays false for the synchronous top level; only the async parent
        // levels mark it, matching ScriptCallStack::buildInspectorObject.
        data.stackTrace->parentStackTrace = gatherAsyncStackTrace(stackTrace->parentStackTrace().get());
    } else if (document && document->scriptableDocumentParser()) {
        data.type = InitiatorType::Parser;
        data.parserURL = document->url().string();
        data.parserLineNumber = protect(document->scriptableDocumentParser())->textPosition().m_line.oneBasedInt();
    }

    if (resourceRequest && instrumentingAgents.persistentDOMAgent())
        data.nodeId = resourceRequest->inspectorInitiatorNodeIdentifier();

    return data;
}

static Inspector::Protocol::Network::Initiator::Type NODELETE toProtocol(InitiatorType type)
{
    switch (type) {
    case InitiatorType::Parser:
        return Inspector::Protocol::Network::Initiator::Type::Parser;
    case InitiatorType::Script:
        return Inspector::Protocol::Network::Initiator::Type::Script;
    case InitiatorType::Other:
        return Inspector::Protocol::Network::Initiator::Type::Other;
    }

    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Network::Initiator::Type::Other;
}

// Reconstructs Protocol::Console::StackTrace, including the async parent chain, from the plain
// InitiatorStackTrace gathered earlier (possibly in another process). This mirrors both
// ScriptCallStack::buildInspectorObject (top level) and AsyncStackTrace::buildInspectorObject
// (parent levels).
static Ref<Inspector::Protocol::Console::StackTrace> buildStackTraceObject(const InitiatorStackTrace& stackTrace)
{
    auto callFrames = JSON::ArrayOf<Inspector::Protocol::Console::CallFrame>::create();
    for (auto& frame : stackTrace.callFrames) {
        callFrames->addItem(Inspector::Protocol::Console::CallFrame::create()
            .setFunctionName(frame.functionName)
            .setUrl(frame.sourceURL)
            .setScriptId(String::number(frame.sourceID))
            .setLineNumber(frame.lineNumber)
            .setColumnNumber(frame.columnNumber)
            .release());
    }

    auto stackTraceObject = Inspector::Protocol::Console::StackTrace::create()
        .setCallFrames(WTF::move(callFrames))
        .release();
    if (stackTrace.truncated)
        stackTraceObject->setTruncated(true);
    if (stackTrace.topCallFrameIsBoundary)
        stackTraceObject->setTopCallFrameIsBoundary(true);
    if (stackTrace.parentStackTrace)
        stackTraceObject->setParentStackTrace(buildStackTraceObject(*stackTrace.parentStackTrace));

    return stackTraceObject;
}

Ref<Inspector::Protocol::Network::Initiator> buildInitiatorObject(const InitiatorData& data)
{
    auto initiatorObject = Inspector::Protocol::Network::Initiator::create()
        .setType(toProtocol(data.type))
        .release();

    switch (data.type) {
    case InitiatorType::Script:
        if (data.stackTrace)
            initiatorObject->setStackTrace(buildStackTraceObject(*data.stackTrace));
        break;
    case InitiatorType::Parser:
        initiatorObject->setUrl(data.parserURL);
        if (data.parserLineNumber)
            initiatorObject->setLineNumber(*data.parserLineNumber);
        break;
    case InitiatorType::Other:
        break;
    }

    if (data.nodeId)
        initiatorObject->setNodeId(*data.nodeId);

    return initiatorObject;
}

} // namespace ResourceUtilities

} // namespace Inspector
