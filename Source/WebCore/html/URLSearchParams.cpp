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

#include "config.h"
#include "URLSearchParams.h"

#include "DOMURL.h"
#include <ranges>
#include <wtf/URLParser.h>

namespace WebCore {

URLSearchParams::URLSearchParams(StringImpl* init, DOMURL* associatedURL)
    : m_associatedURL(associatedURL)
    , m_pairs(startsWith(init, '?') ? WTF::URLParser::parseURLEncodedForm(StringView(init).substring(1)) : WTF::URLParser::parseURLEncodedForm(init))
{
}

URLSearchParams::URLSearchParams(const Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>& pairs)
    : m_pairs(pairs)
{
}

URLSearchParams::~URLSearchParams() = default;

ExceptionOr<Ref<URLSearchParams>> URLSearchParams::create(Variant<Vector<Vector<RefPtr<StringImpl>>>, Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>, RefPtr<StringImpl>>&& variant)
{
    auto visitor = WTF::makeVisitor([&](const Vector<Vector<RefPtr<StringImpl>>>& vector) -> ExceptionOr<Ref<URLSearchParams>> {
        Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>> pairs;
        for (const auto& pair : vector) {
            if (pair.size() != 2)
                return Exception { ExceptionCode::TypeError };
            pairs.append({pair[0], pair[1]});
        }
        return adoptRef(*new URLSearchParams(WTF::move(pairs)));
    }, [&](const Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>& pairs) -> ExceptionOr<Ref<URLSearchParams>> {
        return adoptRef(*new URLSearchParams(pairs));
    }, [&](StringImpl* string) -> ExceptionOr<Ref<URLSearchParams>> {
        return adoptRef(*new URLSearchParams(string, nullptr));
    });
    return WTF::visit(visitor, variant);
}

RefPtr<StringImpl> URLSearchParams::get(StringImpl* name) const
{
    for (const auto& pair : m_pairs) {
        if (equal(pair.key, name))
            return pair.value;
    }
    return nullptr;
}

bool URLSearchParams::has(StringImpl* name, StringImpl* value) const
{
    for (const auto& pair : m_pairs) {
        if (equal(pair.key, name) && (!value || equal(pair.value, value)))
            return true;
    }
    return false;
}

void URLSearchParams::sort()
{
    std::ranges::stable_sort(m_pairs, WTF::codePointCompareLessThan, &PairType::key);
    updateURL();
}

void URLSearchParams::set(StringImpl* name, StringImpl* value)
{
    for (auto& pair : m_pairs) {
        if (!equal(pair.key, name))
            continue;
        if (!equal(pair.value, value))
            pair.value = value;
        bool skippedFirstMatch = false;
        m_pairs.removeAllMatching([&] (const auto& pair) {
            if (equal(pair.key, name)) {
                if (skippedFirstMatch)
                    return true;
                skippedFirstMatch = true;
            }
            return false;
        });
        updateURL();
        return;
    }
    m_pairs.append({name, value});
    updateURL();
}

void URLSearchParams::append(StringImpl* name, StringImpl* value)
{
    m_pairs.append({name, value});
    updateURL();
}

Vector<RefPtr<StringImpl>> URLSearchParams::getAll(StringImpl* name) const
{
    return WTF::compactMap(m_pairs, [&](auto& pair) -> std::optional<RefPtr<StringImpl>> {
        if (equal(pair.key, name))
            return pair.value;
        return std::nullopt;
    });
}

void URLSearchParams::remove(StringImpl* name, StringImpl* value)
{
    m_pairs.removeAllMatching([&] (const auto& pair) {
        return equal(pair.key, name) && (!value || equal(pair.value, value));
    });
    updateURL();
}

RefPtr<StringImpl> URLSearchParams::toString() const
{
    return WTF::URLParser::serialize(m_pairs);
}

void URLSearchParams::updateURL()
{
    if (m_associatedURL)
        m_associatedURL->setSearch(WTF::URLParser::serialize(m_pairs));
}

void URLSearchParams::updateFromAssociatedURL()
{
    ASSERT(m_associatedURL);
    RefPtr<StringImpl> search = m_associatedURL->search();
    m_pairs = startsWith(search, '?') ? WTF::URLParser::parseURLEncodedForm(StringView(search).substring(1)) : WTF::URLParser::parseURLEncodedForm(search);
}

std::optional<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>> URLSearchParams::Iterator::next()
{
    auto& pairs = m_target->pairs();
    if (m_index >= pairs.size())
        return std::nullopt;

    auto& pair = pairs[m_index++];
    return KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>> { pair.key, pair.value };
}

URLSearchParams::Iterator::Iterator(URLSearchParams& params)
    : m_target(params)
{
}
    
}
