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

#pragma once

#include "CachedResource.h"
#include "SVGDocument.h"
#include "TextResourceDecoder.h"

namespace WebCore {

class CachedSVGDocument final : public CachedResource {
public:
    explicit CachedSVGDocument(CachedResourceRequest&&, const PAL::SessionID&, const CookieJar*);
    virtual ~CachedSVGDocument();

    SVGDocument* document() const { return m_document.get(); }

private:
    bool mayTryReplaceEncodedData() const override { return true; }
    void setEncoding(const String&) override;
    String encoding() const override;
    const TextResourceDecoder* textResourceDecoder() const override { return m_decoder.get(); }
    void finishLoading(SharedBuffer*, const NetworkLoadMetrics&) override;

    RefPtr<SVGDocument> m_document;
    RefPtr<TextResourceDecoder> m_decoder;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedSVGDocument, CachedResource::Type::SVGDocumentResource)
