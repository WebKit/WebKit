/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

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
#include "CachedSVGDocument.h"

#include "ParserContentPolicy.h"
#include "Settings.h"
#include "SharedBuffer.h"

namespace WebCore {

CachedSVGDocument::CachedSVGDocument(CachedResourceRequest&& request, PAL::SessionID sessionID, const CookieJar* cookieJar, const Settings& settings)
    : CachedResource(WTF::move(request), Type::SVGDocumentResource, sessionID, cookieJar)
    , m_decoder(TextResourceDecoder::create("application/xml"_s))
    , m_settings(settings)
{
}

CachedSVGDocument::CachedSVGDocument(CachedResourceRequest&& request, CachedSVGDocument& resource)
    : CachedSVGDocument(WTF::move(request), resource.sessionID(), resource.cookieJar(), resource.m_settings)
{
}

CachedSVGDocument::~CachedSVGDocument() = default;

void CachedSVGDocument::setEncoding(const String& chs)
{
    protect(m_decoder)->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

ASCIILiteral CachedSVGDocument::encoding() const
{
    return protect(m_decoder)->encoding().name();
}

void CachedSVGDocument::finishLoading(const FragmentedSharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    if (data) {
        // We don't need to create a new frame because the new document belongs to the parent UseElement.
        Ref document = SVGDocument::create(nullptr, m_settings.copyRef(), response().url());
        document->setMarkupUnsafe(protect(m_decoder)->decodeAndFlush(data->makeContiguous()->span()), { ParserContentPolicy::AllowDeclarativeShadowRoots });
        m_document = WTF::move(document);
    }
    CachedResource::finishLoading(data, metrics);
}

}
