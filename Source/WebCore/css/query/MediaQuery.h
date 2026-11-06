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

#include <WebCore/GenericMediaQueryTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCountable.h>
#include <wtf/RefPtr.h>

namespace WebCore {
namespace MQ {

enum class Prefix : bool { Not, Only };

struct MediaQuery {
    std::optional<Prefix> prefix;
    AtomString mediaType;
    std::optional<Condition> condition { };
};

using MediaQueryList = Vector<MediaQuery>;

// An immutable, reference-counted MediaQueryList class to support cheap copies.
class SharedMediaQueryList {
public:
    SharedMediaQueryList() = default;
    SharedMediaQueryList(MediaQueryList&& list)
    {
        if (!list.isEmpty())
            m_list = RefCountable<MediaQueryList>::create(WTF::move(list));
    }

    const MediaQueryList& operator*() const LIFETIME_BOUND
    {
        static NeverDestroyed<const MediaQueryList> emptyList;
        if (!m_list)
            return emptyList;
        return **m_list;
    }

    bool isEmpty() const { return !m_list; }

    // Identity comparison: true when both handles share the same underlying list (two empty handles are equal).
    friend bool operator==(const SharedMediaQueryList& a, const SharedMediaQueryList& b) { return a.m_list == b.m_list; }

private:
    RefPtr<const RefCountable<MediaQueryList>> m_list;
};

struct MediaQueryResult {
    MediaQueryList mediaQueryList;
    bool result;
};

template<typename TraverseFunction>
void traverseFeatures(const MediaQuery& query, NOESCAPE const TraverseFunction& function)
{
    if (query.condition)
        traverseFeatures(*query.condition, function);
}

template<typename TraverseFunction>
void traverseFeatures(const MediaQueryList& list, NOESCAPE const TraverseFunction& function)
{
    for (auto& query : list)
        traverseFeatures(query, function);
}

}
}
