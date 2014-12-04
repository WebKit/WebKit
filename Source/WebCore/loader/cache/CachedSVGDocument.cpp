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

#include "DocumentLoader.h"
#include "EmptyClients.h"
#include "FrameView.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "Settings.h"
#include "SharedBuffer.h"

namespace WebCore {

CachedSVGDocument::CachedSVGDocument(const ResourceRequest& request, SessionID sessionID)
    : CachedResource(request, SVGDocumentResource, sessionID)
    , m_decoder(TextResourceDecoder::create("application/xml"))
    , m_shouldCreateFrameForDocument(true)
    , m_canReuseResource(true)
{
    setAccept("image/svg+xml");
}

CachedSVGDocument::~CachedSVGDocument()
{
}

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
        // In certain situations (like the scenario when this document belongs to an UseElement) we don't need to create a frame.
        if (m_shouldCreateFrameForDocument) {
            PageConfiguration pageConfiguration;
            fillWithEmptyClients(pageConfiguration);
            
            m_page = Page::createPageFromBuffer(pageConfiguration, data, "image/svg+xml", false, true);
            m_document = downcast<SVGDocument>(m_page->mainFrame().document());
        } else {
            m_document = SVGDocument::create(nullptr, response().url());
            m_document->setContent(m_decoder->decodeAndFlush(data->data(), data->size()));
        }
    }
    CachedResource::finishLoading(data);
}

}
