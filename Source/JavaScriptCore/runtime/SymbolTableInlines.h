/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InferredValueInlines.h"
#include "StructureCreateInlines.h"
#include "SymbolTable.h"

namespace JSC {

inline Structure* SymbolTable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

inline void SymbolTable::finalizeUnconditionally(VM& vm, CollectionScope collectionScope)
{
    m_singleton.finalizeUnconditionally(vm, collectionScope);
}

inline void SymbolTable::notifyCreation(VM& vm, JSScope* scope, const char* reason)
{
    m_singleton.notifyWrite(vm, this, scope, reason);
    if (m_singleton.hasBeenInvalidated()) {
        // Propagate to the original SymbolTable so future clones in other realms pre-invalidate.
        // This "SymbolTable can be reused multiple times for the different lexical environments" is
        // important feedback information from the code execution, and it is derived from the code's lexical
        // characteristics. Thus carrying beyond realms makes sense.
        if (m_propagateCloneInvalidationToOriginal == PropagateCloneInvalidationToOriginal::Yes) {
            if (auto* origin = m_clonedFrom.get()) {
                if (!origin->m_singleton.hasBeenInvalidated())
                    origin->m_singleton.invalidate(vm, StringFireDetail("Singleton invalidated in clone"));
            }
        }
    }
}

inline SymbolTableEntry::SymbolTableEntry(const SymbolTableEntry& other)
    : m_bits(SlimFlag)
{
    *this = other;
}

inline SymbolTableEntry& SymbolTableEntry::operator=(const SymbolTableEntry& other)
{
    if (other.isFat()) [[unlikely]]
        return copySlow(other);
    freeFatEntry();
    m_bits = other.m_bits;
    return *this;
}

inline SymbolTableEntry SymbolTable::get(const ConcurrentJSLocker&, UniquedStringImpl* key)
{
    return m_map.get(key);
}

inline SymbolTableEntry SymbolTable::get(UniquedStringImpl* key)
{
    ConcurrentJSLocker locker(m_lock);
    return get(locker, key);
}

inline SymbolTableEntry SymbolTable::inlineGet(const ConcurrentJSLocker&, UniquedStringImpl* key)
{
    return m_map.inlineGet(key);
}

inline SymbolTableEntry SymbolTable::inlineGet(UniquedStringImpl* key)
{
    ConcurrentJSLocker locker(m_lock);
    return inlineGet(locker, key);
}

inline SymbolTableEntry::FatEntry* SymbolTableEntry::inflate()
{
    if (isFat()) [[likely]]
        return fatEntry();
    return inflateSlow();
}

inline bool SymbolTable::trySetArgumentsLength(VM& vm, uint32_t length)
{
    if (!m_arguments) [[unlikely]] {
        ScopedArgumentsTable* table = ScopedArgumentsTable::tryCreate(vm, length);
        if (!table) [[unlikely]]
            return false;
        m_arguments.set(vm, this, table);
    } else {
        ScopedArgumentsTable* table = m_arguments->trySetLength(vm, length);
        if (!table) [[unlikely]]
            return false;
        m_arguments.set(vm, this, table);
    }

    return true;
}

inline bool SymbolTable::trySetArgumentOffset(VM& vm, uint32_t i, ScopeOffset offset)
{
    ASSERT_WITH_SECURITY_IMPLICATION(m_arguments);
    auto* maybeCloned = m_arguments->trySet(vm, i, offset);
    if (!maybeCloned)
        return false;
    m_arguments.set(vm, this, maybeCloned);
    return true;
}

inline void SymbolTable::prepareToWatchScopedArgument(SymbolTableEntry& entry, uint32_t i)
{
    entry.prepareToWatch();
    if (!m_arguments)
        return;

    WatchpointSet* watchpoints = entry.watchpointSet();
    m_arguments->trySetWatchpointSet(i, watchpoints);
}

} // namespace JSC

