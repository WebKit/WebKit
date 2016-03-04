/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef CachedCSSStyleSheet_h
#define CachedCSSStyleSheet_h

#include "CachedResource.h"
#include <wtf/Vector.h>

namespace WebCore {

    class CachedResourceClient;
    class StyleSheetContents;
    class TextResourceDecoder;
    struct CSSParserContext;

    class CachedCSSStyleSheet final : public CachedResource {
    public:
        CachedCSSStyleSheet(const ResourceRequest&, const String& charset, SessionID);
        virtual ~CachedCSSStyleSheet();

        enum class MIMETypeCheck { Strict, Lax };
        const String sheetText(MIMETypeCheck = MIMETypeCheck::Strict, bool* hasValidMIMEType = nullptr) const;

        PassRefPtr<StyleSheetContents> restoreParsedStyleSheet(const CSSParserContext&, CachePolicy);
        void saveParsedStyleSheet(Ref<StyleSheetContents>&&);

    private:
        bool canUseSheet(MIMETypeCheck, bool* hasValidMIMEType) const;
        bool mayTryReplaceEncodedData() const override { return true; }

        void didAddClient(CachedResourceClient*) override;

        void setEncoding(const String&) override;
        String encoding() const override;
        void finishLoading(SharedBuffer*) override;
        void destroyDecodedData() override;

    protected:
        void checkNotify() override;

        RefPtr<TextResourceDecoder> m_decoder;
        String m_decodedSheetText;

        RefPtr<StyleSheetContents> m_parsedStyleSheetCache;
    };

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedCSSStyleSheet, CachedResource::CSSStyleSheet)

#endif // CachedCSSStyleSheet_h
