/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2017 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CachedCSSStyleSheet.h"

#include "CSSStyleSheet.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceRequest.h"
#include "CachedStyleSheetClient.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MemoryCache.h"
#include "ParsedContentType.h"
#include "SharedBuffer.h"
#include "StyleSheetContents.h"
#include "TextResourceDecoder.h"

namespace WebCore {

CachedCSSStyleSheet::CachedCSSStyleSheet(CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::CSSStyleSheet, sessionID, cookieJar)
    , m_decoder(TextResourceDecoder::create("text/css", request.charset()))
{
}

CachedCSSStyleSheet::~CachedCSSStyleSheet()
{
    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->removedFromMemoryCache();
}

void CachedCSSStyleSheet::didAddClient(CachedResourceClient& client)
{
    ASSERT(client.resourceClientType() == CachedStyleSheetClient::expectedType());
    // CachedResource::didAddClient() must be before setCSSStyleSheet(),
    // because setCSSStyleSheet() may cause scripts to be executed, which could destroy 'c' if it is an instance of HTMLLinkElement.
    // see the comment of HTMLLinkElement::setCSSStyleSheet.
    CachedResource::didAddClient(client);

    if (!isLoading())
        static_cast<CachedStyleSheetClient&>(client).setCSSStyleSheet(m_resourceRequest.url().string(), m_response.url(), m_decoder->encoding().name(), this);
}

void CachedCSSStyleSheet::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedCSSStyleSheet::encoding() const
{
    return m_decoder->encoding().name();
}

const String CachedCSSStyleSheet::sheetText(MIMETypeCheckHint mimeTypeCheckHint, bool* hasValidMIMEType) const
{
    if (!m_data || m_data->isEmpty() || !canUseSheet(mimeTypeCheckHint, hasValidMIMEType))
        return String();

    if (!m_decodedSheetText.isNull())
        return m_decodedSheetText;

    // Don't cache the decoded text, regenerating is cheap and it can use quite a bit of memory
    return m_decoder->decodeAndFlush(m_data->data(), m_data->size());
}

void CachedCSSStyleSheet::setBodyDataFrom(const CachedResource& resource)
{
    ASSERT(resource.type() == type());
    const CachedCSSStyleSheet& sheet = static_cast<const CachedCSSStyleSheet&>(resource);

    CachedResource::setBodyDataFrom(resource);

    m_decoder = sheet.m_decoder;
    m_decodedSheetText = sheet.m_decodedSheetText;
    if (sheet.m_parsedStyleSheetCache)
        saveParsedStyleSheet(*sheet.m_parsedStyleSheetCache);
}

void CachedCSSStyleSheet::finishLoading(SharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    m_data = data;
    setEncodedSize(data ? data->size() : 0);
    // Decode the data to find out the encoding and keep the sheet text around during checkNotify()
    if (data)
        m_decodedSheetText = m_decoder->decodeAndFlush(data->data(), data->size());
    setLoading(false);
    checkNotify(metrics);
    // Clear the decoded text as it is unlikely to be needed immediately again and is cheap to regenerate.
    m_decodedSheetText = String();
}

void CachedCSSStyleSheet::checkNotify(const NetworkLoadMetrics&)
{
    if (isLoading())
        return;

    CachedResourceClientWalker<CachedStyleSheetClient> w(m_clients);
    while (CachedStyleSheetClient* c = w.next())
        c->setCSSStyleSheet(m_resourceRequest.url().string(), m_response.url(), m_decoder->encoding().name(), this);
}

String CachedCSSStyleSheet::responseMIMEType() const
{
    return extractMIMETypeFromMediaType(m_response.httpHeaderField(HTTPHeaderName::ContentType));
}

bool CachedCSSStyleSheet::mimeTypeAllowedByNosniff() const
{
    return parseContentTypeOptionsHeader(m_response.httpHeaderField(HTTPHeaderName::XContentTypeOptions)) != ContentTypeOptionsDisposition::Nosniff || equalLettersIgnoringASCIICase(responseMIMEType(), "text/css");
}

bool CachedCSSStyleSheet::canUseSheet(MIMETypeCheckHint mimeTypeCheckHint, bool* hasValidMIMEType) const
{
    if (errorOccurred())
        return false;

    if (!mimeTypeAllowedByNosniff()) {
        if (hasValidMIMEType)
            *hasValidMIMEType = false;
        return false;
    }

    if (mimeTypeCheckHint == MIMETypeCheckHint::Lax)
        return true;

    // This check exactly matches Firefox.  Note that we grab the Content-Type
    // header directly because we want to see what the value is BEFORE content
    // sniffing.  Firefox does this by setting a "type hint" on the channel.
    // This implementation should be observationally equivalent.
    //
    // This code defaults to allowing the stylesheet for non-HTTP protocols so
    // folks can use standards mode for local HTML documents.
    String mimeType = responseMIMEType();
    bool typeOK = mimeType.isEmpty() || equalLettersIgnoringASCIICase(mimeType, "text/css") || equalLettersIgnoringASCIICase(mimeType, "application/x-unknown-content-type") || !isValidContentType(mimeType);
    if (hasValidMIMEType)
        *hasValidMIMEType = typeOK;
    return typeOK;
}

void CachedCSSStyleSheet::destroyDecodedData()
{
    if (!m_parsedStyleSheetCache)
        return;

    m_parsedStyleSheetCache->removedFromMemoryCache();
    m_parsedStyleSheetCache = nullptr;

    setDecodedSize(0);
}

RefPtr<StyleSheetContents> CachedCSSStyleSheet::restoreParsedStyleSheet(const CSSParserContext& context, CachePolicy cachePolicy, FrameLoader& loader)
{
    if (!m_parsedStyleSheetCache)
        return nullptr;
    if (!m_parsedStyleSheetCache->subresourcesAllowReuse(cachePolicy, loader)) {
        m_parsedStyleSheetCache->removedFromMemoryCache();
        m_parsedStyleSheetCache = nullptr;
        return nullptr;
    }

    ASSERT(m_parsedStyleSheetCache->isCacheable());
    ASSERT(m_parsedStyleSheetCache->isInMemoryCache());

    // Contexts must be identical so we know we would get the same exact result if we parsed again.
    if (m_parsedStyleSheetCache->parserContext() != context)
        return nullptr;

    didAccessDecodedData(MonotonicTime::now());

    return m_parsedStyleSheetCache;
}

void CachedCSSStyleSheet::saveParsedStyleSheet(Ref<StyleSheetContents>&& sheet)
{
    ASSERT(sheet->isCacheable());

    if (m_parsedStyleSheetCache)
        m_parsedStyleSheetCache->removedFromMemoryCache();
    m_parsedStyleSheetCache = WTFMove(sheet);
    m_parsedStyleSheetCache->addedToMemoryCache();

    setDecodedSize(m_parsedStyleSheetCache->estimatedSizeInBytes());
}

}
