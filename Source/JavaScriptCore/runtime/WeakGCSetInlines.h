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

#include "HeapInlines.h"
#include "WeakGCSet.h"
#include "WeakInlines.h"

namespace JSC {

template<typename ValueArg, typename HashArg, typename TraitsArg>
inline WeakGCSet<ValueArg, HashArg, TraitsArg>::WeakGCSet(VM& vm)
    : m_vm(vm)
{
    vm.heap.registerWeakGCHashTable(this);
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
inline WeakGCSet<ValueArg, HashArg, TraitsArg>::~WeakGCSet()
{
    m_vm.heap.unregisterWeakGCHashTable(this);
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
inline typename WeakGCSet<ValueArg, HashArg, TraitsArg>::iterator WeakGCSet<ValueArg, HashArg, TraitsArg>::find(const ValueType& value)
{
    iterator it = m_set.find(value);
    iterator end = m_set.end();
    if (it != end && !*it) // Found a zombie value.
        return end;
    return it;
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
inline typename WeakGCSet<ValueArg, HashArg, TraitsArg>::const_iterator WeakGCSet<ValueArg, HashArg, TraitsArg>::find(const ValueType& value) const
{
    return const_cast<WeakGCSet*>(this)->find(value);
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
inline bool WeakGCSet<ValueArg, HashArg, TraitsArg>::contains(const ValueType& value) const
{
    return find(value) != m_set.end();
}

template<typename ValueArg, typename HashArg, typename TraitsArg>
NEVER_INLINE void WeakGCSet<ValueArg, HashArg, TraitsArg>::pruneStaleEntries()
{
    m_set.removeIf([](auto& entry) {
        return !entry;
    });
}

} // namespace JSC
