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

#include "config.h"
#include "CachedBytecode.h"

#include "CachedTypes.h"
#include "UnlinkedFunctionExecutable.h"
#include <wtf/Function.h>

namespace JSC {

void CachedBytecode::addGlobalUpdate(Ref<CachedBytecode> bytecode)
{
    ASSERT(m_updates.isEmpty());
    m_leafExecutables.clear();
    copyLeafExecutables(bytecode.get());
    m_updates.append(CacheUpdate::GlobalUpdate { WTFMove(bytecode->m_payload) });
}

void CachedBytecode::addFunctionUpdate(const UnlinkedFunctionExecutable* executable, CodeSpecializationKind kind, Ref<CachedBytecode> bytecode)
{
    auto it = m_leafExecutables.find(executable);
    ASSERT(it != m_leafExecutables.end());
    ptrdiff_t offset = it->value.base();
    ASSERT(offset);
    copyLeafExecutables(bytecode.get());
    m_updates.append(CacheUpdate::FunctionUpdate { offset, kind, { executable->features(), executable->hasCapturedVariables() }, WTFMove(bytecode->m_payload) });
}

void CachedBytecode::copyLeafExecutables(const CachedBytecode& bytecode)
{
    for (const auto& it : bytecode.m_leafExecutables) {
        auto addResult = m_leafExecutables.add(it.key, it.value + m_size);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    m_size += bytecode.size();
}

void CachedBytecode::commitUpdates(const ForEachUpdateCallback& callback) const
{
    off_t offset = m_payload.size();
    for (const auto& update : m_updates) {
        const CachePayload* payload = nullptr;
        if (update.isGlobal())
            payload = &update.asGlobal().m_payload;
        else {
            const CacheUpdate::FunctionUpdate& functionUpdate = update.asFunction();
            payload = &functionUpdate.m_payload;
            {
                ptrdiff_t kindOffset = functionUpdate.m_kind == CodeForCall ? CachedFunctionExecutableOffsets::codeBlockForCallOffset() : CachedFunctionExecutableOffsets::codeBlockForConstructOffset();
                ptrdiff_t codeBlockOffset = functionUpdate.m_base + kindOffset + CachedWriteBarrierOffsets::ptrOffset() + CachedPtrOffsets::offsetOffset();
                ptrdiff_t offsetPayload = static_cast<ptrdiff_t>(offset) - codeBlockOffset;
                static_assert(std::is_same<decltype(VariableLengthObjectBase::m_offset), ptrdiff_t>::value, "");
                callback(codeBlockOffset, &offsetPayload, sizeof(ptrdiff_t));
            }

            {
                ptrdiff_t metadataOffset = functionUpdate.m_base + CachedFunctionExecutableOffsets::metadataOffset();
                callback(metadataOffset, &functionUpdate.m_metadata, sizeof(functionUpdate.m_metadata));
            }
        }

        ASSERT(payload);
        callback(offset, payload->data(), payload->size());
        offset += payload->size();
    }
    ASSERT(static_cast<size_t>(offset) == m_size);
}

} // namespace JSC
