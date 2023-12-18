/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

IsoSubspacePerVM::IsoSubspacePerVM(Function<SubspaceParameters(Heap&)> subspaceParameters)
    : m_subspaceParameters(WTFMove(subspaceParameters))
{
}

IsoSubspacePerVM::~IsoSubspacePerVM()
{
    UNREACHABLE_FOR_PLATFORM();
}

IsoSubspace& IsoSubspacePerVM::isoSubspaceforHeap(LockHolder&, JSC::Heap& heap)
{
    auto result = m_subspacePerHeap.add(&heap, nullptr);
    if (result.isNewEntry) {
        SubspaceParameters params = m_subspaceParameters(heap);
        result.iterator->value = new IsoSubspace(params.name, heap, *params.heapCellType, params.size, 0);

        Locker locker { heap.lock() };
        heap.perVMIsoSubspaces.append(this);
    }
    return *result.iterator->value;
}

GCClient::IsoSubspace& IsoSubspacePerVM::clientIsoSubspaceforVM(VM& vm)
{
    Locker locker { m_lock };
    auto result = m_clientSubspacePerVM.add(&vm, nullptr);
    if (!result.isNewEntry && result.iterator->value)
        return *result.iterator->value;

    IsoSubspace& subspace = isoSubspaceforHeap(locker, vm.heap);

    result.iterator->value = new GCClient::IsoSubspace(subspace);
    vm.clientHeap.perVMIsoSubspaces.append(this);
    return *result.iterator->value;
}

void IsoSubspacePerVM::releaseIsoSubspace(JSC::Heap& heap)
{
    IsoSubspace* subspace;
    {
        Locker locker { m_lock };
        subspace = m_subspacePerHeap.take(&heap);
    }
    delete subspace;
}

void IsoSubspacePerVM::releaseClientIsoSubspace(VM& vm)
{
    GCClient::IsoSubspace* clientSubspace;
    {
        Locker locker { m_lock };
        clientSubspace = m_clientSubspacePerVM.take(&vm);
    }
    delete clientSubspace;
}

} // namespace JSC

