/*
 * Copyright (C) 2007-2014 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <algorithm>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class SpaceSplitStringData {
    WTF_MAKE_TZONE_ALLOCATED(SpaceSplitStringData);
    WTF_MAKE_NONCOPYABLE(SpaceSplitStringData);
public:
    static RefPtr<SpaceSplitStringData> create(AtomStringImpl*);

    auto begin() const LIFETIME_BOUND { return std::to_address(tokenArray().begin()); }
    auto end() const LIFETIME_BOUND { return std::to_address(tokenArray().end()); }
    auto begin() LIFETIME_BOUND { return std::to_address(tokenArray().begin()); }
    auto end() LIFETIME_BOUND { return std::to_address(tokenArray().end()); }

    NODELETE bool contains(AtomStringImpl* string) const
    {
        for (auto& token : tokenArray()) {
            if (token == string)
                return true;
        }
        return false;
    }

    NODELETE bool containsAll(const SpaceSplitStringData&) const;

    unsigned size() const { return m_size; }
    static constexpr ptrdiff_t sizeMemoryOffset() { return OBJECT_OFFSETOF(SpaceSplitStringData, m_size); }

    const RefPtr<AtomStringImpl>& operator[](unsigned i) LIFETIME_BOUND { return tokenArray()[i]; }

    void ref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount);
        ++m_refCount;
    }

    void deref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount);
        unsigned tempRefCount = m_refCount - 1;
        if (!tempRefCount) {
            destroy(this);
            return;
        }
        m_refCount = tempRefCount;
    }

    const RefPtr<AtomStringImpl>& keyString() const LIFETIME_BOUND { return m_keyString; }

    static constexpr ptrdiff_t tokensMemoryOffset() { return sizeof(SpaceSplitStringData); }

private:
    static Ref<SpaceSplitStringData> create(AtomStringImpl*, unsigned tokenCount);
    SpaceSplitStringData(AtomStringImpl* string, unsigned size)
        : m_keyString(string)
        , m_refCount(1)
        , m_size(size)
    {
        ASSERT(!isEmpty(string));
        ASSERT_WITH_MESSAGE(m_size, "SpaceSplitStringData should never be empty by definition. There is no difference between empty and null.");
    }

    ~SpaceSplitStringData() = default;
    static void destroy(SpaceSplitStringData*);

    std::span<RefPtr<AtomStringImpl>> tokenArray() LIFETIME_BOUND { return unsafeMakeSpan(m_tokens, m_size); }
    std::span<const RefPtr<AtomStringImpl>> tokenArray() const LIFETIME_BOUND { return unsafeMakeSpan(m_tokens, m_size); }

    RefPtr<AtomStringImpl> m_keyString;
    unsigned m_refCount;
    unsigned m_size;
    RefPtr<AtomStringImpl> m_tokens[0];
};

class SpaceSplitString {
public:
    SpaceSplitString() = default;

    enum class ShouldFoldCase : bool { No, Yes };
    SpaceSplitString(AtomStringImpl*, ShouldFoldCase);

    const RefPtr<AtomStringImpl>& keyString() const LIFETIME_BOUND
    {
        if (m_data)
            return m_data->keyString();
        return nullAtom();
    }

    friend bool operator==(const SpaceSplitString&, const SpaceSplitString&) = default;
    void set(AtomStringImpl*, ShouldFoldCase);
    void clear() { m_data = nullptr; }

    NODELETE bool contains(AtomStringImpl* string) const { return m_data && m_data->contains(string); }
    bool containsAll(const SpaceSplitString& names) const { return !names.m_data || (m_data && m_data->containsAll(*names.m_data)); }

    unsigned size() const { return m_data ? m_data->size() : 0; }
    bool isEmpty() const { return !m_data; }
    const RefPtr<AtomStringImpl>& operator[](unsigned i) const LIFETIME_BOUND
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_data);
        return (*m_data)[i];
    }

    auto begin() const LIFETIME_BOUND { return m_data ? m_data->begin() : nullptr; }
    auto end() const LIFETIME_BOUND { return m_data ? m_data->end() : nullptr; }
    auto begin() LIFETIME_BOUND { return m_data ? m_data->begin() : nullptr; }
    auto end() LIFETIME_BOUND { return m_data ? m_data->end() : nullptr; }

    static bool spaceSplitStringContainsValue(StringView spaceSplitString, StringView value, ShouldFoldCase);

private:
    RefPtr<SpaceSplitStringData> m_data;
};

inline SpaceSplitString::SpaceSplitString(AtomStringImpl* string, ShouldFoldCase shouldFoldCase)
    : m_data(!WTF::isEmpty(string) ? SpaceSplitStringData::create(shouldFoldCase == ShouldFoldCase::Yes ? convertToASCIILowercase(string) : RefPtr { string }) : nullptr)
{
}

} // namespace WebCore
