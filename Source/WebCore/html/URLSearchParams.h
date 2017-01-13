/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DOMURL.h"
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class URLSearchParams : public RefCounted<URLSearchParams> {
public:
    using StringOrURLSearchParams = WTF::Variant<String, RefPtr<URLSearchParams>>;
    static Ref<URLSearchParams> create(const StringOrURLSearchParams&, DOMURL* associatedURL = nullptr);

    void associatedURLDestroyed() { m_associatedURL = nullptr; }
    void append(const String& name, const String& value);
    void remove(const String& name);
    String get(const String& name) const;
    Vector<String> getAll(const String& name) const;
    bool has(const String& name) const;
    void set(const String& name, const String& value);
    String toString() const;
    const Vector<std::pair<String, String>>& pairs() const { return m_pairs; }
    operator const Vector<std::pair<String, String>>&() { return m_pairs; }
    void updateFromAssociatedURL();

    class Iterator {
    public:
        explicit Iterator(URLSearchParams&);
        std::optional<WTF::KeyValuePair<String, String>> next();

    private:
        Ref<URLSearchParams> m_target;
        size_t m_index { 0 };
    };
    Iterator createIterator() { return Iterator { *this }; }

private:
    URLSearchParams(const String&, DOMURL*);
    explicit URLSearchParams(const Vector<std::pair<String, String>>&);
    void updateURL();

    DOMURL* m_associatedURL { nullptr };
    Vector<std::pair<String, String>> m_pairs;
};

inline Ref<URLSearchParams> URLSearchParams::create(const StringOrURLSearchParams& variant, DOMURL* associatedURL)
{
    auto visitor = WTF::makeVisitor([&](const String& string) {
        return adoptRef(*new URLSearchParams(string, associatedURL));
    }, [&](const RefPtr<URLSearchParams>& params) {
        return adoptRef(*new URLSearchParams(static_cast<const Vector<std::pair<String, String>>&>(*params)));
    });
    return WTF::visit(visitor, variant);
}

} // namespace WebCore
