/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "Weak.h"
#include "WeakGCHashTable.h"
#include <wtf/HashSet.h>

namespace JSC {

// A HashSet with Weak<JSCell> values, which automatically removes values once they're garbage collected.

template<typename T>
struct WeakGCSetHashTraits : HashTraits<Weak<T>> {
    static constexpr bool hasIsReleasedWeakValueFunction = true;
    static bool isReleasedWeakValue(const Weak<T>& value)
    {
        return !value.isHashTableDeletedValue() && !value.isHashTableEmptyValue() && !value;
    }
};

template<typename ValueArg, typename HashArg = DefaultHash<Weak<ValueArg>>, typename TraitsArg = WeakGCSetHashTraits<ValueArg>>
class WeakGCSet final : public WeakGCHashTable {
    WTF_MAKE_FAST_ALLOCATED;
    using ValueType = Weak<ValueArg>;
    using HashSetType = HashSet<ValueType, HashArg, TraitsArg>;

public:
    using AddResult = typename HashSetType::AddResult;
    using iterator = typename HashSetType::iterator;
    using const_iterator = typename HashSetType::const_iterator;

    explicit WeakGCSet(VM&);
    ~WeakGCSet() final;

    void clear()
    {
        m_set.clear();
    }

    template<typename HashTranslator, typename T, typename Functor>
    ValueArg* ensureValue(T&& key, Functor&& functor)
    {
        // If functor invokes GC, GC can prune WeakGCSet, and manipulate HashSet while we are touching it in the ensure function.
        // The functor must not invoke GC.
        DisallowGC disallowGC;
        
        auto result = m_set.template ensure<HashTranslator>(std::forward<T>(key), std::forward<Functor>(functor));
        return result.iterator->get();
    }

    inline iterator find(const ValueType& key);
    inline const_iterator find(const ValueType& key) const;
    inline bool contains(const ValueType&) const;

private:
    void pruneStaleEntries() final;

    HashSetType m_set;
    VM& m_vm;
};

} // namespace JSC
