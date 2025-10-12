/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "LLIntPrototypeLoadAdaptiveStructureWatchpoint.h"

#include "CodeBlockInlines.h"
#include "Instruction.h"
#include "JSCellInlines.h"

namespace JSC {

LLIntPrototypeLoadAdaptiveStructureWatchpoint::LLIntPrototypeLoadAdaptiveStructureWatchpoint(CodeBlock* owner, const ObjectPropertyCondition& key, BytecodeIndex bytecodeIndex)
    : Watchpoint(Watchpoint::Type::LLIntPrototypeLoadAdaptiveStructure)
    , m_owner(owner)
    , m_bytecodeIndex(bytecodeIndex)
    , m_key(key)
{
    RELEASE_ASSERT(key.watchingRequiresStructureTransitionWatchpoint());
    RELEASE_ASSERT(!key.watchingRequiresReplacementWatchpoint());
}

LLIntPrototypeLoadAdaptiveStructureWatchpoint::LLIntPrototypeLoadAdaptiveStructureWatchpoint()
    : Watchpoint(Watchpoint::Type::LLIntPrototypeLoadAdaptiveStructure)
    , m_owner(nullptr)
{
}

LLIntPrototypeLoadAdaptiveStructureWatchpoint::~LLIntPrototypeLoadAdaptiveStructureWatchpoint()
{
    ASSERT(!m_owner || !m_owner->wasDestructed());
}

void LLIntPrototypeLoadAdaptiveStructureWatchpoint::initialize(CodeBlock* codeBlock, const ObjectPropertyCondition& key, BytecodeIndex bytecodeOffset)
{
    m_owner = codeBlock;
    m_bytecodeIndex = bytecodeOffset;
    m_key = key;
}

void LLIntPrototypeLoadAdaptiveStructureWatchpoint::install(VM&)
{
    RELEASE_ASSERT(m_key.isWatchable(PropertyCondition::MakeNoChanges));

    m_key.object()->structure()->addTransitionWatchpoint(this);
}

void LLIntPrototypeLoadAdaptiveStructureWatchpoint::fireInternal(VM& vm, const FireDetail&)
{
    ASSERT(!m_owner->wasDestructed());
    if (m_owner->isPendingDestruction())
        return;

    if (m_key.isWatchable(PropertyCondition::EnsureWatchability)) {
        install(vm);
        return;
    }

    auto& instruction = m_owner->instructions().at(m_bytecodeIndex.get().offset());
    switch (instruction->opcodeID()) {
    case op_get_by_id:
        clearLLIntGetByIdCache(instruction->as<OpGetById>().metadata(m_owner.get()).m_modeMetadata);
        break;

    case op_get_length:
        clearLLIntGetByIdCache(instruction->as<OpGetLength>().metadata(m_owner.get()).m_modeMetadata);
        break;

    case op_iterator_open:
        clearLLIntGetByIdCache(instruction->as<OpIteratorOpen>().metadata(m_owner.get()).m_modeMetadata);
        break;

    case op_iterator_next: {
        auto& metadata = instruction->as<OpIteratorNext>().metadata(m_owner.get());
        switch (m_bytecodeIndex.get().checkpoint()) {
        case OpIteratorNext::getDone:
            clearLLIntGetByIdCache(metadata.m_doneModeMetadata);
            break;
        case OpIteratorNext::getValue:
            clearLLIntGetByIdCache(metadata.m_valueModeMetadata);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }

    case op_instanceof: {
        auto& metadata = instruction->as<OpInstanceof>().metadata(m_owner.get());
        switch (m_bytecodeIndex.get().checkpoint()) {
        case OpInstanceof::getHasInstance:
            clearLLIntGetByIdCache(metadata.m_hasInstanceModeMetadata);
            break;
        case OpInstanceof::getPrototype:
            clearLLIntGetByIdCache(metadata.m_prototypeModeMetadata);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void LLIntPrototypeLoadAdaptiveStructureWatchpoint::clearLLIntGetByIdCache(GetByIdModeMetadata& metadata)
{
    metadata.clearToDefaultModeWithoutCache();
}

} // namespace JSC
