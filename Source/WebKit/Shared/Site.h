/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/RegistrableDomain.h>
#include <wtf/HashTraits.h>

namespace WebKit {

// https://html.spec.whatwg.org/multipage/browsers.html#site
class Site {
public:
    explicit Site(const URL&);

    Site(const Site&) = default;
    Site& operator=(const Site&) = default;

    const String& protocol() const { return m_protocol; }
    const WebCore::RegistrableDomain& domain() const { return m_domain; }
    bool isEmpty() const { return m_domain.isEmpty(); }
    bool matches(const URL&) const;

    Site(WTF::HashTableEmptyValueType) { }
    Site(WTF::HashTableDeletedValueType deleted)
        : m_protocol(deleted) { }
    bool isHashTableDeletedValue() const { return m_protocol.isHashTableDeletedValue(); }
    unsigned hash() const;

    bool operator==(const Site&) const = default;
    bool operator!=(const Site&) const = default;

    struct Hash {
        static unsigned hash(const Site& site) { return site.hash(); }
        static bool equal(const Site& a, const Site& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

private:
    String m_protocol;
    WebCore::RegistrableDomain m_domain;
};

}

namespace WTF {
template<> struct DefaultHash<WebKit::Site> : WebKit::Site::Hash { };
template<> struct HashTraits<WebKit::Site> : SimpleClassHashTraits<WebKit::Site> {
    static WebKit::Site emptyValue() { return { WTF::HashTableEmptyValue }; }
};
}
