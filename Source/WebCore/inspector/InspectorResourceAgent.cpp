/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "InspectorResourceAgent.h"

#if ENABLE(INSPECTOR)

#include "Base64.h"
#include "MemoryCache.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTTPHeaderMap.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "KURL.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptCallStack.h"
#include "ScriptCallStackFactory.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"

#include <wtf/CurrentTime.h>
#include <wtf/HexNumber.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace ResourceAgentState {
static const char resourceAgentEnabled[] = "resourceAgentEnabled";
static const char extraRequestHeaders[] = "extraRequestHeaders";
}

void InspectorResourceAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->network();
}

void InspectorResourceAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorResourceAgent::restore()
{
    if (m_state->getBoolean(ResourceAgentState::resourceAgentEnabled))
        enable();
}

void InspectorResourceAgent::resourceContent(ErrorString* errorString, Frame* frame, const KURL& url, String* result)
{
    if (!frame) {
        *errorString = "No frame to get resource content for."; 
        return;
    }

    String textEncodingName;
    RefPtr<SharedBuffer> buffer = InspectorResourceAgent::resourceData(frame, url, &textEncodingName);

    if (buffer) {
        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer->data(), buffer->size());
        return;
    }
    *errorString = "No resource with given URL found."; 
}

void InspectorResourceAgent::resourceContentBase64(ErrorString* errorString, Frame* frame, const KURL& url, String* result)
{
    String textEncodingName;
    RefPtr<SharedBuffer> data = InspectorResourceAgent::resourceData(frame, url, &textEncodingName);
    if (!data) {
        *result = String();
        *errorString = "No resource with given URL found."; 
        return;
    }

    *result = base64Encode(data->data(), data->size());
}

PassRefPtr<SharedBuffer> InspectorResourceAgent::resourceData(Frame* frame, const KURL& url, String* textEncodingName)
{
    FrameLoader* frameLoader = frame->loader();
    DocumentLoader* loader = frameLoader->documentLoader();
    if (equalIgnoringFragmentIdentifier(url, loader->url())) {
        *textEncodingName = frame->document()->inputEncoding();
        return frameLoader->documentLoader()->mainResourceData();
    }

    CachedResource* cachedResource = InspectorResourceAgent::cachedResource(frame, url);
    if (!cachedResource)
        return 0;

    // Zero-sized resources don't have data at all -- so fake the empty buffer, insted of indicating error by returning 0.
    if (!cachedResource->encodedSize())
        return SharedBuffer::create();

    if (cachedResource->isPurgeable()) {
        // If the resource is purgeable then make it unpurgeable to get
        // get its data. This might fail, in which case we return an
        // empty String.
        // FIXME: should we do something else in the case of a purged
        // resource that informs the user why there is no data in the
        // inspector?
        if (!cachedResource->makePurgeable(false))
            return 0;
    }

    *textEncodingName = cachedResource->encoding();
    return cachedResource->data();
}

CachedResource* InspectorResourceAgent::cachedResource(Frame* frame, const KURL& url)
{
    CachedResource* cachedResource = frame->document()->cachedResourceLoader()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url);
    return cachedResource;
}

static PassRefPtr<InspectorObject> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    RefPtr<InspectorObject> headersObject = InspectorObject::create();
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it)
        headersObject->setString(it->first.string(), it->second);
    return headersObject;
}

static PassRefPtr<InspectorObject> buildObjectForTiming(const ResourceLoadTiming& timing)
{
    RefPtr<InspectorObject> timingObject = InspectorObject::create();
    timingObject->setNumber("requestTime", timing.requestTime);
    timingObject->setNumber("proxyStart", timing.proxyStart);
    timingObject->setNumber("proxyEnd", timing.proxyEnd);
    timingObject->setNumber("dnsStart", timing.dnsStart);
    timingObject->setNumber("dnsEnd", timing.dnsEnd);
    timingObject->setNumber("connectStart", timing.connectStart);
    timingObject->setNumber("connectEnd", timing.connectEnd);
    timingObject->setNumber("sslStart", timing.sslStart);
    timingObject->setNumber("sslEnd", timing.sslEnd);
    timingObject->setNumber("sendStart", timing.sendStart);
    timingObject->setNumber("sendEnd", timing.sendEnd);
    timingObject->setNumber("receiveHeadersEnd", timing.receiveHeadersEnd);
    return timingObject;
}

static PassRefPtr<InspectorObject> buildObjectForResourceRequest(const ResourceRequest& request)
{
    RefPtr<InspectorObject> requestObject = InspectorObject::create();
    requestObject->setString("url", request.url().string());
    requestObject->setString("method", request.httpMethod());
    requestObject->setObject("headers", buildObjectForHeaders(request.httpHeaderFields()));
    if (request.httpBody() && !request.httpBody()->isEmpty())
        requestObject->setString("postData", request.httpBody()->flattenToString());
    return requestObject;
}

static PassRefPtr<InspectorObject> buildObjectForResourceResponse(const ResourceResponse& response)
{
    RefPtr<InspectorObject> responseObject = InspectorObject::create();
    if (response.isNull())
        return responseObject;

    responseObject->setNumber("status", response.resourceLoadInfo() ? response.resourceLoadInfo()->httpStatusCode : response.httpStatusCode());
    responseObject->setString("statusText", response.resourceLoadInfo() ? response.resourceLoadInfo()->httpStatusText : response.httpStatusText());
    responseObject->setObject("headers", buildObjectForHeaders(response.resourceLoadInfo() ? response.resourceLoadInfo()->responseHeaders : response.httpHeaderFields()));

    responseObject->setString("mimeType", response.mimeType());
    responseObject->setBoolean("connectionReused", response.connectionReused());
    responseObject->setNumber("connectionID", response.connectionID());
    responseObject->setBoolean("fromDiskCache", response.wasCached());
    if (response.resourceLoadTiming())
        responseObject->setObject("timing", buildObjectForTiming(*response.resourceLoadTiming()));

    if (response.resourceLoadInfo())
        responseObject->setObject("requestHeaders", buildObjectForHeaders(response.resourceLoadInfo()->requestHeaders));

    return responseObject;
}

static String pointerAsId(void* pointer)
{
    unsigned long long address = reinterpret_cast<uintptr_t>(pointer);
    // We want 0 to be "", so that JavaScript checks for if (frameId) worked.
    return String::format("%.0llX", address);
}

static String cachedResourceTypeString(const CachedResource& cachedResource)
{
    switch (cachedResource.type()) {
    case CachedResource::ImageResource:
        return "Image";
    case CachedResource::FontResource:
        return "Font";
    case CachedResource::CSSStyleSheet:
        // Fall through.
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return "Stylesheet";
    case CachedResource::Script:
        return "Script";
    default:
        break;
    }
    return "Other";
}

static PassRefPtr<InspectorObject> buildObjectForCachedResource(const CachedResource& cachedResource)
{
    RefPtr<InspectorObject> resourceObject = InspectorObject::create();
    resourceObject->setString("url", cachedResource.url());
    resourceObject->setString("type", cachedResourceTypeString(cachedResource));
    resourceObject->setNumber("bodySize", cachedResource.encodedSize());
    resourceObject->setObject("response", buildObjectForResourceResponse(cachedResource.response()));
    return resourceObject;
}

InspectorResourceAgent::~InspectorResourceAgent()
{
    ASSERT(!m_instrumentingAgents->inspectorResourceAgent());
}

void InspectorResourceAgent::willSendRequest(unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    RefPtr<InspectorObject> headers = m_state->getObject(ResourceAgentState::extraRequestHeaders);

    if (headers) {
        InspectorObject::const_iterator end = headers->end();
        for (InspectorObject::const_iterator it = headers->begin(); it != end; ++it) {
            String value;
            if (it->second->asString(&value))
                request.setHTTPHeaderField(it->first, value);
        }
    }

    request.setReportLoadTiming(true);
    request.setReportRawHeaders(true);

    RefPtr<ScriptCallStack> callStack = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);
    RefPtr<InspectorArray> callStackValue;
    if (callStack)
        callStackValue = callStack->buildInspectorArray();
    else
        callStackValue = InspectorArray::create();
    m_frontend->willSendRequest(static_cast<int>(identifier), pointerAsId(loader->frame()), pointerAsId(loader), loader->url().string(), buildObjectForResourceRequest(request), buildObjectForResourceResponse(redirectResponse), currentTime(), callStackValue);
}

void InspectorResourceAgent::markResourceAsCached(unsigned long identifier)
{
    m_frontend->markResourceAsCached(static_cast<int>(identifier));
}

void InspectorResourceAgent::didReceiveResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
    RefPtr<InspectorObject> resourceResponse = buildObjectForResourceResponse(response);
    String type = "Other";
    long cachedResourceSize = 0;

    if (loader) {
        CachedResource* cachedResource = InspectorResourceAgent::cachedResource(loader->frame(), response.url());
        if (cachedResource) {
            type = cachedResourceTypeString(*cachedResource);
            cachedResourceSize = cachedResource->encodedSize();
            // Use mime type from cached resource in case the one in response is empty.
            if (response.mimeType().isEmpty())
                resourceResponse->setString("mimeType", cachedResource->response().mimeType());
        }
        if (equalIgnoringFragmentIdentifier(response.url(), loader->frameLoader()->iconURL()))
            type = "Image";
        else if (equalIgnoringFragmentIdentifier(response.url(), loader->url()) && type == "Other")
            type = "Document";
    }
    m_frontend->didReceiveResponse(static_cast<int>(identifier), currentTime(), type, resourceResponse);
    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveContentLength from the network stack.
    if (cachedResourceSize && response.httpStatusCode() == 304)
        didReceiveContentLength(identifier, cachedResourceSize, 0);
}

void InspectorResourceAgent::didReceiveContentLength(unsigned long identifier, int dataLength, int lengthReceived)
{
    m_frontend->didReceiveContentLength(static_cast<int>(identifier), currentTime(), dataLength, lengthReceived);
}

void InspectorResourceAgent::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!finishTime)
        finishTime = currentTime();

    m_frontend->didFinishLoading(static_cast<int>(identifier), finishTime);
}

void InspectorResourceAgent::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    m_frontend->didFailLoading(static_cast<int>(identifier), currentTime(), error.localizedDescription());
}

void InspectorResourceAgent::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* resource)
{
    m_frontend->didLoadResourceFromMemoryCache(pointerAsId(loader->frame()), pointerAsId(loader), loader->url().string(), currentTime(), buildObjectForCachedResource(*resource));
}

void InspectorResourceAgent::setInitialContent(unsigned long identifier, const String& sourceString, const String& type)
{
    m_frontend->setInitialContent(static_cast<int>(identifier), sourceString, type);
}

static PassRefPtr<InspectorObject> buildObjectForFrame(Frame* frame)
{
    RefPtr<InspectorObject> frameObject = InspectorObject::create();
    frameObject->setString("id", pointerAsId(frame));
    frameObject->setString("parentId", pointerAsId(frame->tree()->parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getAttribute(HTMLNames::nameAttr);
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setString("name", name);
    }
    frameObject->setString("url", frame->document()->url().string());
    frameObject->setString("loaderId", pointerAsId(frame->loader()->documentLoader()));

    return frameObject;
}

static PassRefPtr<InspectorObject> buildObjectForFrameTree(Frame* frame)
{
    RefPtr<InspectorObject> result = InspectorObject::create();
    RefPtr<InspectorObject> frameObject = buildObjectForFrame(frame);
    result->setObject("frame", frameObject);

    RefPtr<InspectorArray> subresources = InspectorArray::create();
    result->setArray("resources", subresources);
    const CachedResourceLoader::DocumentResourceMap& allResources = frame->document()->cachedResourceLoader()->allCachedResources();
    CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
    for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
        CachedResource* cachedResource = it->second.get();
        RefPtr<InspectorObject> resourceObject = InspectorObject::create();
        resourceObject->setString("url", cachedResource->url());
        resourceObject->setString("type", cachedResourceTypeString(*cachedResource));
        subresources->pushValue(resourceObject);
    }

    RefPtr<InspectorArray> childrenArray;
    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!childrenArray) {
            childrenArray = InspectorArray::create();
            result->setArray("childFrames", childrenArray);
        }
        childrenArray->pushObject(buildObjectForFrameTree(child));
    }
    return result;
}

void InspectorResourceAgent::didCommitLoad(DocumentLoader* loader)
{
    m_frontend->didCommitLoadForFrame(buildObjectForFrame(loader->frame()), pointerAsId(loader));
}

void InspectorResourceAgent::frameDetachedFromParent(Frame* frame)
{
    m_frontend->frameDetachedFromParent(pointerAsId(frame));
}

#if ENABLE(WEB_SOCKETS)

// FIXME: More this into the front-end?
// Create human-readable binary representation, like "01:23:45:67:89:AB:CD:EF".
static String createReadableStringFromBinary(const unsigned char* value, size_t length)
{
    ASSERT(length > 0);
    StringBuilder builder;
    builder.reserveCapacity(length * 3 - 1);
    for (size_t i = 0; i < length; ++i) {
        if (i > 0)
            builder.append(':');
        appendByteAsHex(value[i], builder);
    }
    return builder.toString();
}

void InspectorResourceAgent::didCreateWebSocket(unsigned long identifier, const KURL& requestURL)
{
    m_frontend->didCreateWebSocket(static_cast<int>(identifier), requestURL.string());
}

void InspectorResourceAgent::willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    RefPtr<InspectorObject> requestObject = InspectorObject::create();
    requestObject->setObject("headers", buildObjectForHeaders(request.headerFields()));
    requestObject->setString("requestKey3", createReadableStringFromBinary(request.key3().value, sizeof(request.key3().value)));
    m_frontend->willSendWebSocketHandshakeRequest(static_cast<int>(identifier), currentTime(), requestObject);
}

void InspectorResourceAgent::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    RefPtr<InspectorObject> responseObject = InspectorObject::create();
    responseObject->setNumber("status", response.statusCode());
    responseObject->setString("statusText", response.statusText());
    responseObject->setObject("headers", buildObjectForHeaders(response.headerFields()));
    responseObject->setString("challengeResponse", createReadableStringFromBinary(response.challengeResponse().value, sizeof(response.challengeResponse().value)));
    m_frontend->didReceiveWebSocketHandshakeResponse(static_cast<int>(identifier), currentTime(), responseObject);
}

void InspectorResourceAgent::didCloseWebSocket(unsigned long identifier)
{
    m_frontend->didCloseWebSocket(static_cast<int>(identifier), currentTime());
}
#endif // ENABLE(WEB_SOCKETS)

Frame* InspectorResourceAgent::frameForId(const String& frameId)
{
    Frame* mainFrame = m_page->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext(mainFrame)) {
        if (pointerAsId(frame) == frameId)
            return frame;
    }
    return 0;
}

void InspectorResourceAgent::enable(ErrorString*)
{
    enable();
}

void InspectorResourceAgent::enable()
{
    if (!m_frontend)
        return;
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, true);
    m_instrumentingAgents->setInspectorResourceAgent(this);
}

void InspectorResourceAgent::disable(ErrorString*)
{
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, false);
    m_instrumentingAgents->setInspectorResourceAgent(0);
}

void InspectorResourceAgent::getCachedResources(ErrorString*, RefPtr<InspectorObject>* object)
{
    *object = buildObjectForFrameTree(m_page->mainFrame());
}

void InspectorResourceAgent::getResourceContent(ErrorString* errorString, const String& frameId, const String& url, bool base64Encode, String* content)
{
    Frame* frame = frameForId(frameId);
    if (!frame) {
        *errorString = "No frame for given id found.";
        return;
    }
    if (base64Encode)
        InspectorResourceAgent::resourceContentBase64(errorString, frame, KURL(ParsedURLString, url), content);
    else
        InspectorResourceAgent::resourceContent(errorString, frame, KURL(ParsedURLString, url), content);
}

void InspectorResourceAgent::setExtraHeaders(ErrorString*, PassRefPtr<InspectorObject> headers)
{
    m_state->setObject(ResourceAgentState::extraRequestHeaders, headers);
}

InspectorResourceAgent::InspectorResourceAgent(InstrumentingAgents* instrumentingAgents, Page* page, InspectorState* state)
    : m_instrumentingAgents(instrumentingAgents)
    , m_page(page)
    , m_state(state)
    , m_frontend(0)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
