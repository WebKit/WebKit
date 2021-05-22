/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "IsoSubspacePerVM.h"

#include "HeapInlines.h"
#include "MarkedSpaceInlines.h"

namespace JSC {

class IsoSubspacePerVM::AutoremovingIsoSubspace final : public IsoSubspace {
public:
    AutoremovingIsoSubspace(IsoSubspacePerVM& perVM, CString name, Heap& heap, HeapCellType* heapCellType, size_t size)
        : IsoSubspace(name, heap, heapCellType, size, /* numberOfLowerTierCells */ 0)
        , m_perVM(perVM)
    {
    }
    
    ~AutoremovingIsoSubspace() final
    {
        Locker locker { m_perVM.m_lock };
        m_perVM.m_subspacePerVM.remove(&space().heap().vm());
    }

private:
    IsoSubspacePerVM& m_perVM;
};

IsoSubspacePerVM::IsoSubspacePerVM(Function<SubspaceParameters(VM&)> subspaceParameters)
    : m_subspaceParameters(WTFMove(subspaceParameters))
{
}

IsoSubspacePerVM::~IsoSubspacePerVM()
{
    UNREACHABLE_FOR_PLATFORM();
}

IsoSubspace& IsoSubspacePerVM::forVM(VM& vm)
{
    Locker locker { m_lock };
    auto result = m_subspacePerVM.add(&vm, nullptr);
    if (result.isNewEntry) {
        SubspaceParameters params = m_subspaceParameters(vm);
        result.iterator->value = new AutoremovingIsoSubspace(*this, params.name, vm.heap, params.heapCellType, params.size);
    }
    return *result.iterator->value;
}

} // namespace JSC

