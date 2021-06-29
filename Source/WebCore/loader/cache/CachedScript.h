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

#pragma once

#include "CachedResource.h"

namespace WebCore {

class TextResourceDecoder;

class CachedScript final : public CachedResource {
public:
    CachedScript(CachedResourceRequest&&, PAL::SessionID, const CookieJar*);
    virtual ~CachedScript();

    StringView script();
    unsigned scriptHash();

private:
    bool mayTryReplaceEncodedData() const final { return true; }

    bool shouldIgnoreHTTPStatusCodeErrors() const final;

    void setEncoding(const String&) final;
    String encoding() const final;
    const TextResourceDecoder* textResourceDecoder() const final { return m_decoder.get(); }
    void finishLoading(SharedBuffer*, const NetworkLoadMetrics&) final;

    void destroyDecodedData() final;

    void setBodyDataFrom(const CachedResource&) final;

    String m_script;
    unsigned m_scriptHash { 0 };

    enum DecodingState { NeverDecoded, DataAndDecodedStringHaveSameBytes, DataAndDecodedStringHaveDifferentBytes };
    DecodingState m_decodingState { NeverDecoded };

    RefPtr<TextResourceDecoder> m_decoder;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedScript, CachedResource::Type::Script)
