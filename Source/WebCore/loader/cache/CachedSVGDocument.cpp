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

#include "SharedBuffer.h"

namespace WebCore {

CachedSVGDocument::CachedSVGDocument(CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::SVGDocumentResource, sessionID, cookieJar)
    , m_decoder(TextResourceDecoder::create("application/xml"))
{
}

CachedSVGDocument::~CachedSVGDocument() = default;

void CachedSVGDocument::setEncoding(const String& chs)
{
    m_decoder->setEncoding(chs, TextResourceDecoder::EncodingFromHTTPHeader);
}

String CachedSVGDocument::encoding() const
{
    return m_decoder->encoding().name();
}

void CachedSVGDocument::finishLoading(SharedBuffer* data)
{
    if (data) {
        // We don't need to create a new frame because the new document belongs to the parent UseElement.
        m_document = SVGDocument::create(sessionID(), nullptr, response().url());
        m_document->setContent(m_decoder->decodeAndFlush(data->data(), data->size()));
    }
    CachedResource::finishLoading(data);
}

}
