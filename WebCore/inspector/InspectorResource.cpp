/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "InspectorResource.h"

#if ENABLE(INSPECTOR)

#include "Cache.h"
#include "CachedResource.h"
#include "DocLoader.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "InspectorFrontend.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "TextEncoding.h"
#include "ScriptObject.h"

namespace WebCore {

InspectorResource::InspectorResource(unsigned long identifier, DocumentLoader* loader, const KURL& requestURL)
    : m_identifier(identifier)
    , m_loader(loader)
    , m_frame(loader->frame())
    , m_requestURL(requestURL)
    , m_expectedContentLength(0)
    , m_cached(false)
    , m_finished(false)
    , m_failed(false)
    , m_length(0)
    , m_responseStatusCode(0)
    , m_startTime(-1.0)
    , m_responseReceivedTime(-1.0)
    , m_endTime(-1.0)
    , m_loadEventTime(-1.0)
    , m_domContentEventTime(-1.0)
    , m_isMainResource(false)
{
}

InspectorResource::~InspectorResource()
{
}

PassRefPtr<InspectorResource> InspectorResource::appendRedirect(unsigned long identifier, const KURL& redirectURL)
{
    // Last redirect is always a container of all previous ones. Pass this container here.
    RefPtr<InspectorResource> redirect = InspectorResource::create(m_identifier, m_loader.get(), redirectURL);
    redirect->m_redirects = m_redirects;
    redirect->m_redirects.append(this);
    redirect->m_changes.set(RedirectsChange);

    m_identifier = identifier;
    // Re-send request info with new id.
    m_changes.set(RequestChange);
    m_redirects.clear();
    return redirect;
}

PassRefPtr<InspectorResource> InspectorResource::createCached(unsigned long identifier, DocumentLoader* loader, const CachedResource* cachedResource)
{
    PassRefPtr<InspectorResource> resource = create(identifier, loader, KURL(ParsedURLString, cachedResource->url()));

    resource->m_finished = true;

    resource->updateResponse(cachedResource->response());

    resource->m_length = cachedResource->encodedSize();
    resource->m_cached = true;
    resource->m_startTime = currentTime();
    resource->m_responseReceivedTime = resource->m_startTime;
    resource->m_endTime = resource->m_startTime;

    resource->m_changes.setAll();

    return resource;
}

void InspectorResource::updateRequest(const ResourceRequest& request)
{
    m_requestHeaderFields = request.httpHeaderFields();
    m_requestMethod = request.httpMethod();
    if (request.httpBody() && !request.httpBody()->isEmpty())
        m_requestFormData = request.httpBody()->flattenToString();

    m_changes.set(RequestChange);
}

void InspectorResource::updateResponse(const ResourceResponse& response)
{
    m_expectedContentLength = response.expectedContentLength();
    m_mimeType = response.mimeType();
    if (m_mimeType.isEmpty() && response.httpStatusCode() == 304) {
        CachedResource* cachedResource = cache()->resourceForURL(response.url().string());
        if (cachedResource)
            m_mimeType = cachedResource->response().mimeType();
    }
    m_responseHeaderFields = response.httpHeaderFields();
    m_responseStatusCode = response.httpStatusCode();
    m_suggestedFilename = response.suggestedFilename();

    m_changes.set(ResponseChange);
    m_changes.set(TypeChange);
}

static void populateHeadersObject(ScriptObject* object, const HTTPHeaderMap& headers)
{
    HTTPHeaderMap::const_iterator end = headers.end();
    for (HTTPHeaderMap::const_iterator it = headers.begin(); it != end; ++it) {
        object->set(it->first.string(), it->second);
    }
}


void InspectorResource::updateScriptObject(InspectorFrontend* frontend)
{
    if (m_changes.hasChange(NoChange))
        return;

    ScriptObject jsonObject = frontend->newScriptObject();
    if (m_changes.hasChange(RequestChange)) {
        jsonObject.set("url", m_requestURL.string());
        jsonObject.set("documentURL", m_frame->document()->url().string());
        jsonObject.set("host", m_requestURL.host());
        jsonObject.set("path", m_requestURL.path());
        jsonObject.set("lastPathComponent", m_requestURL.lastPathComponent());
        ScriptObject requestHeaders = frontend->newScriptObject();
        populateHeadersObject(&requestHeaders, m_requestHeaderFields);
        jsonObject.set("requestHeaders", requestHeaders);
        jsonObject.set("mainResource", m_isMainResource);
        jsonObject.set("requestMethod", m_requestMethod);
        jsonObject.set("requestFormData", m_requestFormData);
        jsonObject.set("didRequestChange", true);
        jsonObject.set("cached", m_cached);
    }

    if (m_changes.hasChange(ResponseChange)) {
        jsonObject.set("mimeType", m_mimeType);
        jsonObject.set("suggestedFilename", m_suggestedFilename);
        jsonObject.set("expectedContentLength", m_expectedContentLength);
        jsonObject.set("statusCode", m_responseStatusCode);
        jsonObject.set("suggestedFilename", m_suggestedFilename);
        ScriptObject responseHeaders = frontend->newScriptObject();
        populateHeadersObject(&responseHeaders, m_responseHeaderFields);
        jsonObject.set("responseHeaders", responseHeaders);
        jsonObject.set("didResponseChange", true);
    }

    if (m_changes.hasChange(TypeChange)) {
        jsonObject.set("type", static_cast<int>(type()));
        jsonObject.set("didTypeChange", true);
    }

    if (m_changes.hasChange(LengthChange)) {
        jsonObject.set("resourceSize", m_length);
        jsonObject.set("didLengthChange", true);
    }

    if (m_changes.hasChange(CompletionChange)) {
        jsonObject.set("failed", m_failed);
        jsonObject.set("finished", m_finished);
        jsonObject.set("didCompletionChange", true);
    }

    if (m_changes.hasChange(TimingChange)) {
        if (m_startTime > 0)
            jsonObject.set("startTime", m_startTime);
        if (m_responseReceivedTime > 0)
            jsonObject.set("responseReceivedTime", m_responseReceivedTime);
        if (m_endTime > 0)
            jsonObject.set("endTime", m_endTime);
        if (m_loadEventTime > 0)
            jsonObject.set("loadEventTime", m_loadEventTime);
        if (m_domContentEventTime > 0)
            jsonObject.set("domContentEventTime", m_domContentEventTime);
        jsonObject.set("didTimingChange", true);
    }

    if (m_changes.hasChange(RedirectsChange)) {
        for (size_t i = 0; i < m_redirects.size(); ++i)
            m_redirects[i]->updateScriptObject(frontend);
    }

    if (frontend->updateResource(m_identifier, jsonObject))
        m_changes.clearAll();
}

void InspectorResource::releaseScriptObject(InspectorFrontend* frontend)
{
    m_changes.setAll();

    for (size_t i = 0; i < m_redirects.size(); ++i)
        m_redirects[i]->releaseScriptObject(frontend);

    if (frontend)
        frontend->removeResource(m_identifier);
}

CachedResource* InspectorResource::cachedResource() const
{
    // Try hard to find a corresponding CachedResource. During preloading, DocLoader may not have the resource in document resources set yet,
    // but Inspector will already try to fetch data that is only available via CachedResource (and it won't update once the resource is added,
    // because m_changes will not have the appropriate bits set).
    const String& url = m_requestURL.string();
    CachedResource* cachedResource = m_frame->document()->docLoader()->cachedResource(url);
    if (!cachedResource)
        cachedResource = cache()->resourceForURL(url);
    return cachedResource;
}

InspectorResource::Type InspectorResource::cachedResourceType() const
{
    CachedResource* cachedResource = this->cachedResource();

    if (!cachedResource)
        return Other;

    switch (cachedResource->type()) {
        case CachedResource::ImageResource:
            return Image;
        case CachedResource::FontResource:
            return Font;
        case CachedResource::CSSStyleSheet:
#if ENABLE(XSLT)
        case CachedResource::XSLStyleSheet:
#endif
            return Stylesheet;
        case CachedResource::Script:
            return Script;
        default:
            return Other;
    }
}

InspectorResource::Type InspectorResource::type() const
{
    if (!m_xmlHttpResponseText.isNull())
        return XHR;

    if (m_requestURL == m_loader->requestURL()) {
        InspectorResource::Type resourceType = cachedResourceType();
        if (resourceType == Other)
            return Doc;

        return resourceType;
    }

    if (m_loader->frameLoader() && m_requestURL == m_loader->frameLoader()->iconURL())
        return Image;

    return cachedResourceType();
}

void InspectorResource::setXMLHttpResponseText(const ScriptString& data)
{
    m_xmlHttpResponseText = data;
    m_changes.set(TypeChange);
}

String InspectorResource::sourceString() const
{
    if (!m_xmlHttpResponseText.isNull())
        return String(m_xmlHttpResponseText);

    String textEncodingName;
    RefPtr<SharedBuffer> buffer = resourceData(&textEncodingName);
    if (!buffer)
        return String();

    TextEncoding encoding(textEncodingName);
    if (!encoding.isValid())
        encoding = WindowsLatin1Encoding();
    return encoding.decode(buffer->data(), buffer->size());
}

PassRefPtr<SharedBuffer> InspectorResource::resourceData(String* textEncodingName) const
{
    if (m_requestURL == m_loader->requestURL()) {
        *textEncodingName = m_frame->document()->inputEncoding();
        return m_loader->mainResourceData();
    }

    CachedResource* cachedResource = this->cachedResource();
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

void InspectorResource::startTiming()
{
    m_startTime = currentTime();
    m_changes.set(TimingChange);
}

void InspectorResource::markResponseReceivedTime()
{
    m_responseReceivedTime = currentTime();
    m_changes.set(TimingChange);
}

void InspectorResource::endTiming()
{
    m_endTime = currentTime();
    m_finished = true;
    m_changes.set(TimingChange);
    m_changes.set(CompletionChange);
}

void InspectorResource::markDOMContentEventTime()
{
    m_domContentEventTime = currentTime();
    m_changes.set(TimingChange);
}

void InspectorResource::markLoadEventTime()
{
    m_loadEventTime = currentTime();
    m_changes.set(TimingChange);
}

void InspectorResource::markFailed()
{
    m_failed = true;
    m_changes.set(CompletionChange);
}

void InspectorResource::addLength(int lengthReceived)
{
    m_length += lengthReceived;
    m_changes.set(LengthChange);

    // Update load time, otherwise the resource will
    // have start time == end time and  0 load duration
    // until its loading is completed.
    m_endTime = currentTime();
    m_changes.set(TimingChange);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
