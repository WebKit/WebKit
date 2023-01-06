/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SecurityOrigin.h"
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>

namespace WebCore {

class ScopedURL : public URL {
public:
    ScopedURL(URL&& url)
        : ScopedURL { WTFMove(url), SecurityOrigin::emptyOrigin() } { }
    ScopedURL(String&& url)
        : ScopedURL { url } { }
    ScopedURL(const URL& url)
        : ScopedURL { URL { url } } { }
    ScopedURL(const URL& base, const String& relative, const WTF::URLTextEncoding* encoding = nullptr)
        : ScopedURL { URL { base, relative, encoding } } { }
    explicit ScopedURL(const String& absoluteURL, const WTF::URLTextEncoding* encoding = nullptr)
        : ScopedURL { URL { absoluteURL, encoding } } { }
    ScopedURL()
        : ScopedURL { URL { } } { }

    bool operator==(const ScopedURL&) const;
    bool operator==(const URL&) const;

#if USE(CF)
    ScopedURL(CFURLRef cfURL)
        : ScopedURL { URL { cfURL } } { }
    RetainPtr<CFURLRef> createCFURL() const { return URL::createCFURL(); }

#endif

#if USE(FOUNDATION)
    ScopedURL(NSURL *cocoaURL)
        : ScopedURL { URL { cocoaURL } } { }
    operator NSURL *() const { return URL::operator NSURL *(); }
#endif

#if USE(GLIB) && HAVE(GURI)
    ScopedURL(GUri* gURI) : ScopedURL { URL { gURI } } { };
    GRefPtr<GUri> createGUri() const { return URL::createGUri(); }
#endif

    ScopedURL isolatedCopy() const & { return { URL::isolatedCopy(), m_topOrigin->isolatedCopy() }; }
    ScopedURL isolatedCopy() && { return { WTFMove(static_cast<URL&>(*this)).isolatedCopy(), m_topOrigin->isolatedCopy() }; }

    String toString() const { return String { string() + ":"_s + m_topOrigin->toString() }; }

    URL& asURL() { return *this; }
    const URL& asURL() const { return *this; }

    Ref<SecurityOrigin> topOrigin() const { return m_topOrigin; }

private:

    ScopedURL(URL&& url, Ref<SecurityOrigin> topOrigin)
        : URL { WTFMove(url) }, m_topOrigin { topOrigin } { }
    Ref<SecurityOrigin> m_topOrigin;
};

inline void add(Hasher& hasher, const ScopedURL& url)
{
    add(hasher, url.string(), url.topOrigin());
}

inline bool ScopedURL::operator==(const URL& other) const
{
    return asURL() == other;
}

inline bool ScopedURL::operator==(const ScopedURL& other) const
{
    return asURL() == other.asURL() && m_topOrigin == other.m_topOrigin;
}

struct ScopedURLKeyHash {
    static unsigned hash(const WebCore::ScopedURL& key) { return computeHash(key); }
    static bool equal(const WebCore::ScopedURL& a, const WebCore::ScopedURL& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

} // namespace WebCore

namespace WTF {


template<> struct HashTraits<WebCore::ScopedURL> : GenericHashTraits<WebCore::ScopedURL> {
    static WebCore::ScopedURL emptyValue() { return { }; }
    static void constructDeletedValue(WebCore::ScopedURL& slot)
    {
        new (NotNull, static_cast<URL*>(&slot)) URL(WTF::HashTableDeletedValue);
    }

    static bool isDeletedValue(const WebCore::ScopedURL& slot) { return slot.isHashTableDeletedValue(); }
};

template<> struct DefaultHash<WebCore::ScopedURL> : WebCore::ScopedURLKeyHash { };

} // namespace WTF
