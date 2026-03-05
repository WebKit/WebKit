/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ScopedArgumentsTable.h"

#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

const ClassInfo ScopedArgumentsTable::s_info = { "ScopedArgumentsTable"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ScopedArgumentsTable) };

ScopedArgumentsTable::ScopedArgumentsTable(VM& vm)
    : Base(vm, vm.scopedArgumentsTableStructure.get())
    , m_length(0)
    , m_locked(false)
{
}

ScopedArgumentsTable::~ScopedArgumentsTable() = default;

void ScopedArgumentsTable::destroy(JSCell* cell)
{
    static_cast<ScopedArgumentsTable*>(cell)->ScopedArgumentsTable::~ScopedArgumentsTable();
}

ScopedArgumentsTable* ScopedArgumentsTable::create(VM& vm)
{
    ScopedArgumentsTable* result =
        new (NotNull, allocateCell<ScopedArgumentsTable>(vm)) ScopedArgumentsTable(vm);
    result->finishCreation(vm);
    return result;
}

ScopedArgumentsTable* ScopedArgumentsTable::tryCreate(VM& vm, uint32_t length)
{
    void* buffer = tryAllocateCell<ScopedArgumentsTable>(vm);
    if (!buffer) [[unlikely]]
        return nullptr;
    ScopedArgumentsTable* result = new (NotNull, buffer) ScopedArgumentsTable(vm);
    result->finishCreation(vm);

    result->m_length = length;
    result->m_arguments = ArgumentsPtr::tryCreate(length);
    if (!result->m_arguments) [[unlikely]]
        return nullptr;
    result->m_watchpointSets.fill(nullptr, length);
    return result;
}

ScopedArgumentsTable* ScopedArgumentsTable::tryClone(VM& vm)
{
    ScopedArgumentsTable* result = tryCreate(vm, m_length);
    if (!result) [[unlikely]]
        return nullptr;
    for (unsigned i = m_length; i--;)
        result->at(i) = this->at(i);
    result->m_watchpointSets = this->m_watchpointSets;
    return result;
}

ScopedArgumentsTable* ScopedArgumentsTable::trySetLength(VM& vm, uint32_t newLength)
{
    if (!m_locked) [[likely]] {
        ArgumentsPtr newArguments = ArgumentsPtr::tryCreate(newLength, newLength);
        if (!newArguments) [[unlikely]]
            return nullptr;
        for (unsigned i = std::min(m_length, newLength); i--;)
            newArguments.at(i) = this->at(i);
        m_length = newLength;
        m_arguments = WTF::move(newArguments);
        m_watchpointSets.resize(newLength);
        return this;
    }
    
    ScopedArgumentsTable* result = tryCreate(vm, newLength);
    if (!result) [[unlikely]]
        return nullptr;
    m_watchpointSets.resize(newLength);
    for (unsigned i = std::min(m_length, newLength); i--;) {
        result->at(i) = this->at(i);
        result->m_watchpointSets[i] = this->m_watchpointSets[i];
    }
    return result;
}

static_assert(std::is_trivially_destructible<ScopeOffset>::value);

ScopedArgumentsTable* ScopedArgumentsTable::trySet(VM& vm, uint32_t i, ScopeOffset value)
{
    ScopedArgumentsTable* result;
    if (m_locked) [[unlikely]] {
        result = tryClone(vm);
        if (!result) [[unlikely]]
            return nullptr;
    } else
        result = this;
    result->at(i) = value;
    return result;
}

void ScopedArgumentsTable::trySetWatchpointSet(uint32_t i, WatchpointSet* watchpoints)
{
    if (!watchpoints)
        return;

    if (i >= m_watchpointSets.size())
        return;

    m_watchpointSets[i] = watchpoints;
}

Structure* ScopedArgumentsTable::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
