/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InspectorValues.h"
#include "KURL.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "WebSocketHandshakeRequest.h"
#include "WebSocketHandshakeResponse.h"

#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuffer.h>

namespace WebCore {

bool InspectorResourceAgent::resourceContent(Frame* frame, const KURL& url, String* result)
{
    if (!frame)
        return false;

    String textEncodingName;
    RefPtr<SharedBuffer> buffer = InspectorResourceAgent::resourceData(frame, url, &textEncodingName);

    if (buffer) {
        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer->data(), buffer->size());
        return true;
    }

    return false;
}

bool InspectorResourceAgent::resourceContentBase64(Frame* frame, const KURL& url, String* result)
{
    Vector<char> out;
    String textEncodingName;
    RefPtr<SharedBuffer> data = InspectorResourceAgent::resourceData(frame, url, &textEncodingName);
    if (!data) {
        *result = String();
        return false;
    }

    base64Encode(data->buffer(), out);
    *result = String(out.data(), out.size());
    return true;
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
    const String& urlString = url.string();
    CachedResource* cachedResource = frame->document()->cachedResourceLoader()->cachedResource(urlString);
    if (!cachedResource)
        cachedResource = cache()->resourceForURL(urlString);
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
    requestObject->setString("httpMethod", request.httpMethod());
    requestObject->setObject("httpHeaderFields", buildObjectForHeaders(request.httpHeaderFields()));
    if (request.httpBody() && !request.httpBody()->isEmpty())
        requestObject->setString("requestFormData", request.httpBody()->flattenToString());
    return requestObject;
}

static PassRefPtr<InspectorObject> buildObjectForResourceResponse(const ResourceResponse& response)
{
    RefPtr<InspectorObject> responseObject = InspectorObject::create();
    if (response.isNull()) {
        responseObject->setBoolean("isNull", true);
        return responseObject;
    }
    responseObject->setString("url", response.url().string());
    responseObject->setString("mimeType", response.mimeType());
    responseObject->setNumber("expectedContentLength", response.expectedContentLength());
    responseObject->setString("textEncodingName", response.textEncodingName());
    responseObject->setString("suggestedFilename", response.suggestedFilename());
    responseObject->setNumber("httpStatusCode", response.httpStatusCode());
    responseObject->setString("httpStatusText", response.httpStatusText());
    responseObject->setObject("httpHeaderFields", buildObjectForHeaders(response.httpHeaderFields()));
    responseObject->setBoolean("connectionReused", response.connectionReused());
    responseObject->setNumber("connectionID", response.connectionID());
    responseObject->setBoolean("wasCached", response.wasCached());
    if (response.resourceLoadTiming())
        responseObject->setObject("timing", buildObjectForTiming(*response.resourceLoadTiming()));
    if (response.resourceLoadInfo()) {
        RefPtr<InspectorObject> loadInfoObject = InspectorObject::create();
        loadInfoObject->setNumber("httpStatusCode", response.resourceLoadInfo()->httpStatusCode);
        loadInfoObject->setString("httpStatusText", response.resourceLoadInfo()->httpStatusText);
        loadInfoObject->setObject("requestHeaders", buildObjectForHeaders(response.resourceLoadInfo()->requestHeaders));
        loadInfoObject->setObject("responseHeaders", buildObjectForHeaders(response.resourceLoadInfo()->responseHeaders));
        responseObject->setObject("loadInfo", loadInfoObject);
    }
    return responseObject;
}

static unsigned long frameId(Frame* frame)
{
    return reinterpret_cast<uintptr_t>(frame);
}

static PassRefPtr<InspectorObject> buildObjectForDocumentLoader(DocumentLoader* loader)
{
    RefPtr<InspectorObject> documentLoaderObject = InspectorObject::create();
    documentLoaderObject->setNumber("frameId", frameId(loader->frame()));
    documentLoaderObject->setNumber("loaderId", reinterpret_cast<uintptr_t>(loader));
    documentLoaderObject->setString("url", loader->requestURL().string());
    return documentLoaderObject;
}

static PassRefPtr<InspectorObject> buildObjectForFrameResource(Frame* frame)
{
    FrameLoader* frameLoader = frame->loader();
    DocumentLoader* loader = frameLoader->documentLoader();

    RefPtr<InspectorObject> resourceObject = InspectorObject::create();
    resourceObject->setString("url", loader->url().string());
    resourceObject->setObject("loader", buildObjectForDocumentLoader(loader));
    resourceObject->setObject("request", buildObjectForResourceRequest(loader->request()));
    resourceObject->setObject("response", buildObjectForResourceResponse(loader->response()));
    return resourceObject;
}

static String cachedResourceTypeString(const CachedResource& cachedResource)
{
    switch (cachedResource.type()) {
    case CachedResource::ImageResource:
        return "Image";
        break;
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
        return "Other";
    }
}

static PassRefPtr<InspectorObject> buildObjectForCachedResource(DocumentLoader* loader, const CachedResource& cachedResource)
{
    RefPtr<InspectorObject> resourceObject = InspectorObject::create();
    resourceObject->setString("url", cachedResource.url());
    resourceObject->setString("type", cachedResourceTypeString(cachedResource));
    resourceObject->setNumber("encodedSize", cachedResource.encodedSize());
    resourceObject->setObject("response", buildObjectForResourceResponse(cachedResource.response()));
    resourceObject->setObject("loader", buildObjectForDocumentLoader(loader));
    return resourceObject;
}

static void populateObjectWithFrameResources(Frame* frame, PassRefPtr<InspectorObject> frameResources)
{
    frameResources->setObject("resource", buildObjectForFrameResource(frame));
    RefPtr<InspectorArray> subresources = InspectorArray::create();
    frameResources->setArray("subresources", subresources);

    const CachedResourceLoader::DocumentResourceMap& allResources = frame->document()->cachedResourceLoader()->allCachedResources();
    CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
    for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
        CachedResource* cachedResource = it->second.get();
        RefPtr<InspectorObject> cachedResourceObject = buildObjectForCachedResource(frame->loader()->documentLoader(), *cachedResource);
        subresources->pushValue(cachedResourceObject);
    }
}

InspectorResourceAgent::~InspectorResourceAgent()
{
}

void InspectorResourceAgent::identifierForInitialRequest(unsigned long identifier, const KURL& url, DocumentLoader* loader)
{
    RefPtr<InspectorObject> loaderObject = buildObjectForDocumentLoader(loader);
    m_frontend->identifierForInitialRequest(identifier, url.string(), loaderObject);
}

void InspectorResourceAgent::willSendRequest(unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    m_frontend->willSendRequest(identifier, currentTime(), buildObjectForResourceRequest(request), buildObjectForResourceResponse(redirectResponse));
}

void InspectorResourceAgent::markResourceAsCached(unsigned long identifier)
{
    m_frontend->markResourceAsCached(identifier);
}

void InspectorResourceAgent::didReceiveResponse(unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
    RefPtr<InspectorObject> resourceResponse = buildObjectForResourceResponse(response);
    String type = "Other";
    if (loader) {
        if (equalIgnoringFragmentIdentifier(response.url(), loader->frameLoader()->iconURL()))
            type = "Image";
        else {
            CachedResource* cachedResource = InspectorResourceAgent::cachedResource(loader->frame(), response.url());
            if (cachedResource)
                type = cachedResourceTypeString(*cachedResource);

            if (equalIgnoringFragmentIdentifier(response.url(), loader->url()) && type == "Other")
                type = "Document";

            // Use mime type from cached resource in case the one in response is empty.
            if (response.mimeType().isEmpty() && cachedResource)
                resourceResponse->setString("mimeType", cachedResource->response().mimeType());
        }
    }
    m_frontend->didReceiveResponse(identifier, currentTime(), type, resourceResponse);
}

void InspectorResourceAgent::didReceiveContentLength(unsigned long identifier, int lengthReceived)
{
    m_frontend->didReceiveContentLength(identifier, currentTime(), lengthReceived);
}

void InspectorResourceAgent::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!finishTime)
        finishTime = currentTime();

    m_frontend->didFinishLoading(identifier, finishTime);
}

void InspectorResourceAgent::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    m_frontend->didFailLoading(identifier, currentTime(), error.localizedDescription());
}

void InspectorResourceAgent::didLoadResourceFromMemoryCache(DocumentLoader* loader, const CachedResource* resource)
{
    m_frontend->didLoadResourceFromMemoryCache(currentTime(), buildObjectForCachedResource(loader, *resource));
}

void InspectorResourceAgent::setInitialContent(unsigned long identifier, const String& sourceString, const String& type)
{
    m_frontend->setInitialContent(identifier, sourceString, type);
}

static PassRefPtr<InspectorObject> buildObjectForFrame(Frame* frame)
{
    RefPtr<InspectorObject> frameObject = InspectorObject::create();
    frameObject->setNumber("id", frameId(frame));
    frameObject->setNumber("parentId", frameId(frame->tree()->parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getAttribute(HTMLNames::nameAttr);
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setString("name", name);
    }
    frameObject->setString("url", frame->loader()->url().string());
    return frameObject;
}

static PassRefPtr<InspectorObject> buildObjectForFrameTree(Frame* frame, bool dumpResources)
{
    RefPtr<InspectorObject> frameObject = buildObjectForFrame(frame);

    if (dumpResources)
        populateObjectWithFrameResources(frame, frameObject);
    RefPtr<InspectorArray> childrenArray;
    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!childrenArray) {
            childrenArray = InspectorArray::create();
            frameObject->setArray("children", childrenArray);
        }
        childrenArray->pushObject(buildObjectForFrameTree(child, dumpResources));
    }
    return frameObject;
}

void InspectorResourceAgent::didCommitLoad(DocumentLoader* loader)
{
    m_frontend->didCommitLoadForFrame(buildObjectForFrame(loader->frame()), buildObjectForDocumentLoader(loader));
}

void InspectorResourceAgent::frameDetachedFromParent(Frame* frame)
{
    m_frontend->frameDetachedFromParent(frameId(frame));
}

#if ENABLE(WEB_SOCKETS)

// FIXME: More this into the front-end?
// Create human-readable binary representation, like "01:23:45:67:89:AB:CD:EF".
static String createReadableStringFromBinary(const unsigned char* value, size_t length)
{
    ASSERT(length > 0);
    static const char hexDigits[17] = "0123456789ABCDEF";
    size_t bufferSize = length * 3 - 1;
    StringBuffer buffer(bufferSize);
    size_t index = 0;
    for (size_t i = 0; i < length; ++i) {
        if (i > 0)
            buffer[index++] = ':';
        buffer[index++] = hexDigits[value[i] >> 4];
        buffer[index++] = hexDigits[value[i] & 0xF];
    }
    ASSERT(index == bufferSize);
    return String::adopt(buffer);
}

void InspectorResourceAgent::didCreateWebSocket(unsigned long identifier, const KURL& requestURL)
{
    m_frontend->didCreateWebSocket(identifier, requestURL.string());
}

void InspectorResourceAgent::willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    RefPtr<InspectorObject> requestObject = InspectorObject::create();
    requestObject->setObject("webSocketHeaderFields", buildObjectForHeaders(request.headerFields()));
    requestObject->setString("webSocketRequestKey3", createReadableStringFromBinary(request.key3().value, sizeof(request.key3().value)));
    m_frontend->willSendWebSocketHandshakeRequest(identifier, currentTime(), requestObject);
}

void InspectorResourceAgent::didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    RefPtr<InspectorObject> responseObject = InspectorObject::create();
    responseObject->setNumber("statusCode", response.statusCode());
    responseObject->setString("statusText", response.statusText());
    responseObject->setObject("webSocketHeaderFields", buildObjectForHeaders(response.headerFields()));
    responseObject->setString("webSocketChallengeResponse", createReadableStringFromBinary(response.challengeResponse().value, sizeof(response.challengeResponse().value)));
    m_frontend->didReceiveWebSocketHandshakeResponse(identifier, currentTime(), responseObject);
}

void InspectorResourceAgent::didCloseWebSocket(unsigned long identifier)
{
    m_frontend->didCloseWebSocket(identifier, currentTime());
}
#endif // ENABLE(WEB_SOCKETS)

Frame* InspectorResourceAgent::frameForId(unsigned long frameId)
{
    Frame* mainFrame = m_page->mainFrame();
    for (Frame* frame = mainFrame; frame; frame = frame->tree()->traverseNext(mainFrame)) {
        if (reinterpret_cast<uintptr_t>(frame) == frameId)
            return frame;
    }
    return 0;
}

void InspectorResourceAgent::cachedResources(RefPtr<InspectorObject>* object)
{
    *object = buildObjectForFrameTree(m_page->mainFrame(), true);
}

void InspectorResourceAgent::resourceContent(unsigned long id, const String& url, bool base64Encode, String* content)
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext(m_page->mainFrame())) {
        if (frameId(frame) != id)
            continue;
        if (base64Encode)
            InspectorResourceAgent::resourceContentBase64(frame, KURL(ParsedURLString, url), content);
        else
            InspectorResourceAgent::resourceContent(frame, KURL(ParsedURLString, url), content);
        break;
    }
}

InspectorResourceAgent::InspectorResourceAgent(Page* page, InspectorFrontend* frontend)
    : m_page(page)
    , m_frontend(frontend)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
