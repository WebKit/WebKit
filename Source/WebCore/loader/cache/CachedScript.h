/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2025 Apple Inc. All rights reserved.

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
#include <wtf/ScopedLambda.h>
#include <wtf/ThreadAssertions.h>

namespace WebCore {

class TextResourceDecoder;

enum class ScriptTrackingPrivacyProtectionsEnabled : bool { No, Yes };

class CachedScript final : public CachedResource {
public:
    CachedScript(CachedResourceRequest&&, PAL::SessionID, const CookieJar*, ScriptTrackingPrivacyProtectionsEnabled);
    virtual ~CachedScript();

    enum class ShouldDecodeAsUTF8Only : bool { No, Yes };
    WEBCORE_EXPORT StringView script(ShouldDecodeAsUTF8Only = ShouldDecodeAsUTF8Only::No);
    WEBCORE_EXPORT unsigned scriptHash(ShouldDecodeAsUTF8Only = ShouldDecodeAsUTF8Only::No);
    // Like script(), but safe on any thread. The text is valid only during the call.
    WEBCORE_EXPORT void withScriptConcurrently(ShouldDecodeAsUTF8Only, const ScopedLambda<void(StringView)>&);

    bool requiresPrivacyProtections() const { return m_requiresPrivacyProtections; }

private:
    bool mayTryReplaceEncodedData() const final { return true; }

    void setEncoding(const String&) final;
    ASCIILiteral encoding() const final WTF_REQUIRES_SHARED_LOCK(m_lock);
    const TextResourceDecoder* textResourceDecoder() const final WTF_REQUIRES_SHARED_LOCK(m_lock) { return m_decoder.get(); }
    void finishLoading(const FragmentedSharedBuffer*, const NetworkLoadMetrics&) final;

    void destroyDecodedData() final;

    void setBodyDataFrom(const CachedResource&) final;

    // m_script, m_wasForceDecodedAsUTF8, m_decodingState and m_decoder are written on the main thread
    // under m_lock and read on other threads by withScriptConcurrently(), which locks; the main
    // thread's own reads use assertIsOwnerThread() instead of locking. m_scriptHash is not read off
    // the main thread, so it needs no guard.
    String m_script WTF_GUARDED_BY_LOCK(m_lock);
    unsigned m_scriptHash { 0 };
    bool m_wasForceDecodedAsUTF8 WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_requiresPrivacyProtections { false };

    enum DecodingState : uint8_t { NeverDecoded, DataAndDecodedStringHaveSameBytes, DataAndDecodedStringHaveDifferentBytes };
    DecodingState m_decodingState WTF_GUARDED_BY_LOCK(m_lock) { NeverDecoded };

    mutable Lock m_lock;

    RefPtr<TextResourceDecoder> m_decoder WTF_GUARDED_BY_LOCK(m_lock);

    WTF_DECLARE_OWNER_THREAD_ASSERTIONS(m_lock, mainThreadLike);
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CachedScript) \
    static bool isType(const WebCore::CachedResource& resource) { return resource.type() == WebCore::CachedResource::Type::Script || resource.type() == WebCore::CachedResource::Type::JSON || resource.type() == WebCore::CachedResource::Type::Text; } \
SPECIALIZE_TYPE_TRAITS_END()
