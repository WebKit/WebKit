/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#pragma once

#include "CachedResource.h"

namespace WebCore {

class FrameLoader;
class StyleSheetContents;
class TextResourceDecoder;

struct CSSParserContext;

class CachedCSSStyleSheet final : public CachedResource {
public:
    CachedCSSStyleSheet(CachedResourceRequest&&, const PAL::SessionID&, const CookieJar*);
    virtual ~CachedCSSStyleSheet();

    enum class MIMETypeCheckHint { Strict, Lax };
    const String sheetText(MIMETypeCheckHint = MIMETypeCheckHint::Strict, bool* hasValidMIMEType = nullptr) const;

    RefPtr<StyleSheetContents> restoreParsedStyleSheet(const CSSParserContext&, CachePolicy, FrameLoader&);
    void saveParsedStyleSheet(Ref<StyleSheetContents>&&);

    bool mimeTypeAllowedByNosniff() const;

private:
    String responseMIMEType() const;
    bool canUseSheet(MIMETypeCheckHint, bool* hasValidMIMEType) const;
    bool mayTryReplaceEncodedData() const final { return true; }

    void didAddClient(CachedResourceClient&) final;

    void setEncoding(const String&) final;
    String encoding() const final;
    const TextResourceDecoder* textResourceDecoder() const final { return m_decoder.get(); }
    void finishLoading(SharedBuffer*, const NetworkLoadMetrics&) final;
    void destroyDecodedData() final;

    void setBodyDataFrom(const CachedResource&) final;

    void checkNotify(const NetworkLoadMetrics&) final;

    RefPtr<TextResourceDecoder> m_decoder;
    String m_decodedSheetText;

    RefPtr<StyleSheetContents> m_parsedStyleSheetCache;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedCSSStyleSheet, CachedResource::Type::CSSStyleSheet)
