/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "ConcatKeyAtomStringCache.h"
#include "Identifier.h"
#include "SmallStrings.h"
#include "VM.h"

namespace JSC {

template<typename Func>
inline JSString* ConcatKeyAtomStringCache::getOrInsert(VM& vm, JSString* s0, JSString* s1, JSString* s2, const Func& func)
{
    JSString* variable = nullptr;
    switch (m_mode) {
    case Mode::Variable0: {
        variable = s0;
        break;
    }
    case Mode::Variable1: {
        variable = s1;
        break;
    }
    case Mode::Variable2: {
        variable = s2;
        break;
    }
    case Mode::Megamorphic: {
        return nullptr;
    }
    }

    auto value = variable->tryGetValue();
    SUPPRESS_UNCOUNTED_LOCAL auto* impl = value->impl();

    if (!impl)
        return nullptr;

    if (!impl->isAtom())
        return nullptr;

    SUPPRESS_UNCOUNTED_LOCAL auto& atomStringImpl = *static_cast<AtomStringImpl*>(impl);
    if (atomStringImpl.length() > maxStringLengthForCache)
        return nullptr;


    if (auto* result = m_cache.get(atomStringImpl))
        return result;

    if (auto* result = func(vm)) [[likely]] {
        size_t size = m_cache.size();
        if (size == maxCapacity) [[unlikely]] {
            {
                Locker locker { m_lock };
                m_cache.clear();
            }
            m_mode = Mode::Megamorphic;
        } else {
            {
                Locker locker { m_lock };
                m_cache.add(atomStringImpl, result);
            }
            vm.writeBarrier(m_owner, result);
            if (size < 2) {
                auto& entry = m_quickCache[size];
                entry.m_key.set(vm, m_owner, variable);
                entry.m_value.set(vm, m_owner, result);
            }
        }
        return result;
    }

    return nullptr;
}

} // namespace JSC
