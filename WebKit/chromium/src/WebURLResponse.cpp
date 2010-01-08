/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebURLResponse.h"

#include "ResourceResponse.h"

#include "WebHTTPHeaderVisitor.h"
#include "WebString.h"
#include "WebURL.h"
#include "WebURLResponsePrivate.h"

using namespace WebCore;

namespace WebKit {

// The standard implementation of WebURLResponsePrivate, which maintains
// ownership of a ResourceResponse instance.
class WebURLResponsePrivateImpl : public WebURLResponsePrivate {
public:
    WebURLResponsePrivateImpl()
    {
        m_resourceResponse = &m_resourceResponseAllocation;
    }

    WebURLResponsePrivateImpl(const WebURLResponsePrivate* p)
        : m_resourceResponseAllocation(*p->m_resourceResponse)
    {
        m_resourceResponse = &m_resourceResponseAllocation;
    }

    virtual void dispose() { delete this; }

    ResourceResponse m_resourceResponseAllocation;
};

void WebURLResponse::initialize()
{
    assign(new WebURLResponsePrivateImpl());
}

void WebURLResponse::reset()
{
    assign(0);
}

void WebURLResponse::assign(const WebURLResponse& r)
{
    if (&r != this)
        assign(r.m_private ? new WebURLResponsePrivateImpl(r.m_private) : 0);
}

bool WebURLResponse::isNull() const
{
    return !m_private || m_private->m_resourceResponse->isNull();
}

WebURL WebURLResponse::url() const
{
    return m_private->m_resourceResponse->url();
}

void WebURLResponse::setURL(const WebURL& url)
{
    m_private->m_resourceResponse->setURL(url);
}

WebString WebURLResponse::mimeType() const
{
    return m_private->m_resourceResponse->mimeType();
}

void WebURLResponse::setMIMEType(const WebString& mimeType)
{
    m_private->m_resourceResponse->setMimeType(mimeType);
}

long long WebURLResponse::expectedContentLength() const
{
    return m_private->m_resourceResponse->expectedContentLength();
}

void WebURLResponse::setExpectedContentLength(long long expectedContentLength)
{
    m_private->m_resourceResponse->setExpectedContentLength(expectedContentLength);
}

WebString WebURLResponse::textEncodingName() const
{
    return m_private->m_resourceResponse->textEncodingName();
}

void WebURLResponse::setTextEncodingName(const WebString& textEncodingName)
{
    m_private->m_resourceResponse->setTextEncodingName(textEncodingName);
}

WebString WebURLResponse::suggestedFileName() const
{
    return m_private->m_resourceResponse->suggestedFilename();
}

void WebURLResponse::setSuggestedFileName(const WebString& suggestedFileName)
{
    m_private->m_resourceResponse->setSuggestedFilename(suggestedFileName);
}

int WebURLResponse::httpStatusCode() const
{
    return m_private->m_resourceResponse->httpStatusCode();
}

void WebURLResponse::setHTTPStatusCode(int httpStatusCode)
{
    m_private->m_resourceResponse->setHTTPStatusCode(httpStatusCode);
}

WebString WebURLResponse::httpStatusText() const
{
    return m_private->m_resourceResponse->httpStatusText();
}

void WebURLResponse::setHTTPStatusText(const WebString& httpStatusText)
{
    m_private->m_resourceResponse->setHTTPStatusText(httpStatusText);
}

WebString WebURLResponse::httpHeaderField(const WebString& name) const
{
    return m_private->m_resourceResponse->httpHeaderField(name);
}

void WebURLResponse::setHTTPHeaderField(const WebString& name, const WebString& value)
{
    m_private->m_resourceResponse->setHTTPHeaderField(name, value);
}

void WebURLResponse::addHTTPHeaderField(const WebString& name, const WebString& value)
{
    // FIXME: Add an addHTTPHeaderField method to ResourceResponse.
    const HTTPHeaderMap& map = m_private->m_resourceResponse->httpHeaderFields();
    String valueStr(value);
    pair<HTTPHeaderMap::iterator, bool> result =
        const_cast<HTTPHeaderMap*>(&map)->add(name, valueStr);
    if (!result.second)
        result.first->second += ", " + valueStr;
}

void WebURLResponse::clearHTTPHeaderField(const WebString& name)
{
    // FIXME: Add a clearHTTPHeaderField method to ResourceResponse.
    const HTTPHeaderMap& map = m_private->m_resourceResponse->httpHeaderFields();
    const_cast<HTTPHeaderMap*>(&map)->remove(name);
}

void WebURLResponse::visitHTTPHeaderFields(WebHTTPHeaderVisitor* visitor) const
{
    const HTTPHeaderMap& map = m_private->m_resourceResponse->httpHeaderFields();
    for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
        visitor->visitHeader(it->first, it->second);
}

double WebURLResponse::lastModifiedDate() const
{
    return static_cast<double>(m_private->m_resourceResponse->lastModifiedDate());
}

void WebURLResponse::setLastModifiedDate(double lastModifiedDate)
{
    m_private->m_resourceResponse->setLastModifiedDate(static_cast<time_t>(lastModifiedDate));
}

bool WebURLResponse::isContentFiltered() const
{
    return m_private->m_resourceResponse->isContentFiltered();
}

void WebURLResponse::setIsContentFiltered(bool isContentFiltered)
{
    m_private->m_resourceResponse->setIsContentFiltered(isContentFiltered);
}

long long WebURLResponse::appCacheID() const
{
    return m_private->m_resourceResponse->appCacheID();
}

void WebURLResponse::setAppCacheID(long long appCacheID)
{
    m_private->m_resourceResponse->setAppCacheID(appCacheID);
}

WebURL WebURLResponse::appCacheManifestURL() const
{
    return m_private->m_resourceResponse->appCacheManifestURL();
}

void WebURLResponse::setAppCacheManifestURL(const WebURL& url)
{
    m_private->m_resourceResponse->setAppCacheManifestURL(url);
}

WebCString WebURLResponse::securityInfo() const
{
    // FIXME: getSecurityInfo is misnamed.
    return m_private->m_resourceResponse->getSecurityInfo();
}

void WebURLResponse::setSecurityInfo(const WebCString& securityInfo)
{
    m_private->m_resourceResponse->setSecurityInfo(securityInfo);
}

ResourceResponse& WebURLResponse::toMutableResourceResponse()
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceResponse);

    return *m_private->m_resourceResponse;
}

const ResourceResponse& WebURLResponse::toResourceResponse() const
{
    ASSERT(m_private);
    ASSERT(m_private->m_resourceResponse);

    return *m_private->m_resourceResponse;
}

bool WebURLResponse::wasFetchedViaSPDY() const
{
    return m_private->m_resourceResponse->wasFetchedViaSPDY();
}

void WebURLResponse::setWasFetchedViaSPDY(bool value)
{
    m_private->m_resourceResponse->setWasFetchedViaSPDY(value);
}

void WebURLResponse::assign(WebURLResponsePrivate* p)
{
    // Subclasses may call this directly so a self-assignment check is needed
    // here as well as in the public assign method.
    if (m_private == p)
        return;
    if (m_private)
        m_private->dispose();
    m_private = p;
}

} // namespace WebKit
