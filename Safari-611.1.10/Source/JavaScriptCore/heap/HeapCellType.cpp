/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "HeapCellType.h"

#include "JSCInlines.h"
#include "MarkedBlockInlines.h"

namespace JSC {

// Writing it this way ensures that when you pass this as a functor, the callee is specialized for
// this callback. If you wrote this as a normal function then the callee would be specialized for
// the function's type and it would have indirect calls to that function. And unlike a lambda, it's
// possible to mark this ALWAYS_INLINE.
struct DefaultDestroyFunc {
    ALWAYS_INLINE void operator()(VM& vm, JSCell* cell) const
    {
        ASSERT(cell->structureID());
        Structure* structure = cell->structure(vm);
        ASSERT(structure->typeInfo().structureIsImmortal());
        const ClassInfo* classInfo = structure->classInfo();
        MethodTable::DestroyFunctionPtr destroy = classInfo->methodTable.destroy;
        destroy(cell);
    }
};

HeapCellType::HeapCellType(CellAttributes attributes)
    : m_attributes(attributes)
{
}

HeapCellType::~HeapCellType()
{
}

void HeapCellType::finishSweep(MarkedBlock::Handle& block, FreeList* freeList)
{
    block.finishSweepKnowingHeapCellType(freeList, DefaultDestroyFunc());
}

void HeapCellType::destroy(VM& vm, JSCell* cell)
{
    DefaultDestroyFunc()(vm, cell);
}

} // namespace JSC

