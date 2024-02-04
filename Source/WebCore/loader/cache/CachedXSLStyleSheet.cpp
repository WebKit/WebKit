/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedXSLStyleSheet.h"

#include "CachedResourceClientWalker.h"
#include "CachedStyleSheetClient.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace WebCore {

#if ENABLE(XSLT)

CachedXSLStyleSheet::CachedXSLStyleSheet(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::XSLStyleSheet, sessionID, cookieJar)
    , m_decoder(TextResourceDecoder::create("text/xsl"_s))
{
}

CachedXSLStyleSheet::~CachedXSLStyleSheet() = default;

RefPtr<TextResourceDecoder> CachedXSLStyleSheet::protectedDecoder() const
{
    return m_decoder;
}

void CachedXSLStyleSheet::didAddClient(CachedResourceClient& client)
{
    ASSERT(client.resourceClientType() == CachedStyleSheetClient::expectedType());
    if (!isLoading())
        downcast<CachedStyleSheetClient>(client).setXSLStyleSheet(m_resourceRequest.url().string(), response().url(), m_sheet);
}

void CachedXSLStyleSheet::setEncoding(const String& chs)
{
    protectedDecoder()->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedXSLStyleSheet::encoding() const
{
    return String::fromLatin1(protectedDecoder()->encoding().name());
}

void CachedXSLStyleSheet::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (data) {
        Ref contiguousData = data->makeContiguous();
        setEncodedSize(data->size());
        m_sheet = protectedDecoder()->decodeAndFlush(contiguousData->data(), encodedSize());
        m_data = WTFMove(contiguousData);
    } else {
        m_data = nullptr;
        setEncodedSize(0);
    }
    setLoading(false);
    checkNotify(metrics);
}

void CachedXSLStyleSheet::checkNotify(const NetworkLoadMetrics&)
{
    if (isLoading())
        return;
    
    CachedResourceClientWalker<CachedStyleSheetClient> walker(*this);
    while (CachedStyleSheetClient* c = walker.next())
        c->setXSLStyleSheet(m_resourceRequest.url().string(), response().url(), m_sheet);
}

#endif

}
