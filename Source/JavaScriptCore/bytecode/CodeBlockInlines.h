/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
#include "CodeBlock.h"
#include "UnlinkedMetadataTableInlines.h"

namespace JSC {

template<typename Functor>
void CodeBlock::forEachValueProfile(const Functor& func)
{
    for (unsigned i = 0; i < numberOfArgumentValueProfiles(); ++i)
        func(valueProfileForArgument(i));

    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.profile); });

        FOR_EACH_OPCODE_WITH_VALUE_PROFILE(VISIT)

#undef VISIT
    }

}

template<typename Functor>
void CodeBlock::forEachArrayProfile(const Functor& func)
{
    if (m_metadata) {
        m_metadata->forEach<OpGetById>([&] (auto& metadata) {
            if (metadata.mode == GetByIdMode::ArrayLength)
                func(metadata.modeMetadata.arrayLengthMode.arrayProfile);
        });

#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.arrayProfile); });

        FOR_EACH_OPCODE_WITH_ARRAY_PROFILE(VISIT)

#undef VISIT
    }
}

template<typename Functor>
void CodeBlock::forEachArrayAllocationProfile(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.arrayAllocationProfile); });

        FOR_EACH_OPCODE_WITH_ARRAY_ALLOCATION_PROFILE(VISIT)

#undef VISIT
    }
}

template<typename Functor>
void CodeBlock::forEachObjectAllocationProfile(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.objectAllocationProfile); });

        FOR_EACH_OPCODE_WITH_OBJECT_ALLOCATION_PROFILE(VISIT)

#undef VISIT
    }
}

template<typename Functor>
void CodeBlock::forEachLLIntCallLinkInfo(const Functor& func)
{
    if (m_metadata) {
#define VISIT(__op) \
    m_metadata->forEach<__op>([&] (auto& metadata) { func(metadata.callLinkInfo); });

        FOR_EACH_OPCODE_WITH_LLINT_CALL_LINK_INFO(VISIT)

#undef VISIT
    }
}

} // namespace JSC
