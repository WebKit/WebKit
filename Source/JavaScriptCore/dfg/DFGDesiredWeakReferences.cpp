/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DFGDesiredWeakReferences.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGCommonData.h"
#include "JSCJSValueInlines.h"
#include "WriteBarrier.h"

namespace JSC { namespace DFG {

DesiredWeakReferences::DesiredWeakReferences()
    : m_codeBlock(nullptr)
{
}

DesiredWeakReferences::DesiredWeakReferences(CodeBlock* codeBlock)
    : m_codeBlock(codeBlock)
{
}

DesiredWeakReferences::~DesiredWeakReferences() = default;

void DesiredWeakReferences::addLazily(JSCell* cell)
{
    if (cell) {
        if (Structure* structure = jsDynamicCast<Structure*>(cell))
            m_structures.add(structure->id());
        else {
            // There are weird relationships in how optimized CodeBlocks
            // point to other CodeBlocks. We don't want to have them be
            // part of the weak pointer set. For example, an optimized CodeBlock
            // having a weak pointer to itself will cause it to get collected.
            RELEASE_ASSERT(!jsDynamicCast<CodeBlock*>(cell));
            m_cells.add(cell);
        }
    }
}

void DesiredWeakReferences::addLazily(JSValue value)
{
    if (value.isCell())
        addLazily(value.asCell());
}

bool DesiredWeakReferences::contains(JSCell* cell)
{
    if (Structure* structure = jsDynamicCast<Structure*>(cell))
        return m_structures.contains(structure->id());
    return m_cells.contains(cell);
}

void DesiredWeakReferences::finalize()
{
    m_finalizedCells = FixedVector<WriteBarrier<JSCell>>(m_cells.size());
    {
        unsigned index = 0;
        for (JSCell* target : m_cells)
            m_finalizedCells[index++].setWithoutWriteBarrier(target);
    }
    m_finalizedStructures = FixedVector<StructureID>(m_structures.size());
    {
        unsigned index = 0;
        for (StructureID structureID : m_structures)
            m_finalizedStructures[index++] = structureID;
    }
}

void DesiredWeakReferences::reallyAdd(VM& vm, CommonData* common)
{
    // We do not emit WriteBarrier here since (1) GC is deferred and (2) we emit write-barrier on CodeBlock when finishing DFG::Plan::reallyAdd.
    ASSERT_UNUSED(vm, vm.heap.isDeferred());
    if (!m_finalizedCells.isEmpty() || !m_finalizedStructures.isEmpty()) {
        ASSERT(common->m_weakStructureReferences.isEmpty());
        ASSERT(common->m_weakReferences.isEmpty());
        // This is just moving a pointer. And we already synchronized with Lock etc. with compiler threads.
        // So at this point, these vectors are fully constructed and baked by the compiler threads.
        // We can just move these pointers to CommonData, and that's enough.
        static_assert(sizeof(m_finalizedStructures) == sizeof(void*));
        static_assert(sizeof(m_finalizedCells) == sizeof(void*));
        common->m_weakStructureReferences = WTFMove(m_finalizedStructures);
        common->m_weakReferences = WTFMove(m_finalizedCells);
    }
}

template<typename Visitor>
void DesiredWeakReferences::visitChildren(Visitor& visitor)
{
    for (JSCell* target : m_cells)
        visitor.appendUnbarriered(target);
    for (StructureID structureID : m_structures)
        visitor.appendUnbarriered(structureID.decode());
}

template void DesiredWeakReferences::visitChildren(AbstractSlotVisitor&);
template void DesiredWeakReferences::visitChildren(SlotVisitor&);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
