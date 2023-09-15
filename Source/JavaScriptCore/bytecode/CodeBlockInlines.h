/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#include "BaselineJITCode.h"
#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "UnlinkedMetadataTableInlines.h"

namespace JSC {

template<typename Functor>
void CodeBlock::forEachValueProfile(const Functor& func)
{
    for (unsigned i = 0; i < numberOfArgumentValueProfiles(); ++i)
        func(valueProfileForArgument(i), true);

    if (m_metadata) {
        auto wrapper = [&] (ValueProfile& profile) {
            func(profile, false);
        };
        m_metadata->forEachValueProfile(wrapper);
    }
}

template<typename Functor>
void CodeBlock::forEachArrayAllocationProfile(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.m_arrayAllocationProfile); });

        FOR_EACH_OPCODE_WITH_ARRAY_ALLOCATION_PROFILE(VISIT)

#undef VISIT
    }
}

template<typename Functor>
void CodeBlock::forEachObjectAllocationProfile(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.m_objectAllocationProfile); });

        FOR_EACH_OPCODE_WITH_OBJECT_ALLOCATION_PROFILE(VISIT)

#undef VISIT
    }
}

template<typename Functor>
void CodeBlock::forEachLLIntOrBaselineCallLinkInfo(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.m_callLinkInfo); });

        FOR_EACH_OPCODE_WITH_CALL_LINK_INFO(VISIT)

#undef VISIT
    }
}

#if ENABLE(JIT)
ALWAYS_INLINE const JITCodeMap& CodeBlock::jitCodeMap()
{
    ASSERT(jitType() == JITType::BaselineJIT);
    return static_cast<BaselineJITCode*>(m_jitCode.get())->m_jitCodeMap;
}

ALWAYS_INLINE SimpleJumpTable& CodeBlock::baselineSwitchJumpTable(int tableIndex)
{
    ASSERT(jitType() == JITType::BaselineJIT);
    return static_cast<BaselineJITCode*>(m_jitCode.get())->m_switchJumpTables[tableIndex];
}

ALWAYS_INLINE StringJumpTable& CodeBlock::baselineStringSwitchJumpTable(int tableIndex)
{
    ASSERT(jitType() == JITType::BaselineJIT);
    return static_cast<BaselineJITCode*>(m_jitCode.get())->m_stringSwitchJumpTables[tableIndex];
}
#endif

#if ENABLE(DFG_JIT)
ALWAYS_INLINE SimpleJumpTable& CodeBlock::dfgSwitchJumpTable(int tableIndex)
{
    ASSERT(jitType() == JITType::DFGJIT);
    return static_cast<DFG::JITCode*>(m_jitCode.get())->m_switchJumpTables[tableIndex];
}

ALWAYS_INLINE StringJumpTable& CodeBlock::dfgStringSwitchJumpTable(int tableIndex)
{
    ASSERT(jitType() == JITType::DFGJIT);
    return static_cast<DFG::JITCode*>(m_jitCode.get())->m_stringSwitchJumpTables[tableIndex];
}
#endif

} // namespace JSC
