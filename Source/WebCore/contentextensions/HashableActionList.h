/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/Hasher.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

struct HashableActionList {
    enum DeletedValueTag { DeletedValue };
    explicit HashableActionList(DeletedValueTag) { state = Deleted; }

    enum EmptyValueTag { EmptyValue };
    explicit HashableActionList(EmptyValueTag) { state = Empty; }

    template<typename AnyVectorType>
    explicit HashableActionList(const AnyVectorType& otherActions)
        : actions(otherActions)
        , state(Valid)
    {
        std::sort(actions.begin(), actions.end());
        StringHasher hasher;
        hasher.addCharactersAssumingAligned(reinterpret_cast<const UChar*>(actions.data()), actions.size() * sizeof(uint64_t) / sizeof(UChar));
        hash = hasher.hash();
    }

    bool isEmptyValue() const { return state == Empty; }
    bool isDeletedValue() const { return state == Deleted; }

    bool operator==(const HashableActionList& other) const
    {
        return state == other.state && actions == other.actions;
    }

    bool operator!=(const HashableActionList& other) const
    {
        return !(*this == other);
    }

    Vector<uint64_t> actions;
    unsigned hash;
    enum {
        Valid,
        Empty,
        Deleted
    } state;
};

struct HashableActionListHash {
    static unsigned hash(const HashableActionList& actionKey)
    {
        return actionKey.hash;
    }

    static bool equal(const HashableActionList& a, const HashableActionList& b)
    {
        return a == b;
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct HashableActionListHashTraits : public WTF::CustomHashTraits<HashableActionList> {
    static const bool emptyValueIsZero = false;
};

} // namespace ContentExtensions
} // namespace WebCore
