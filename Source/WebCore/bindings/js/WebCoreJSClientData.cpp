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
#include "WebCoreJSClientData.h"

#include "JSDOMBinding.h"
#include <heap/FastMallocAlignedMemoryAllocator.h>
#include <heap/HeapInlines.h>
#include <heap/MarkingConstraint.h>
#include <heap/MarkedAllocatorInlines.h>
#include <heap/MarkedBlockInlines.h>
#include <heap/SubspaceInlines.h>
#include <heap/VisitingTimeout.h>
#include <runtime/VM.h>
#include <wtf/MainThread.h>

using namespace JSC;

namespace WebCore {

JSVMClientData::JSVMClientData(VM& vm)
    : m_builtinFunctions(vm)
    , m_builtinNames(&vm)
    , m_outputConstraintSpace("WebCore Wrapper w/ Output Constraint", vm.heap, vm.fastMallocAllocator.get())
    , m_globalObjectOutputConstraintSpace("WebCore Global Object w/ Output Constraint", vm.heap, vm.fastMallocAllocator.get())
{
}

JSVMClientData::~JSVMClientData()
{
    ASSERT(m_worldSet.contains(m_normalWorld.get()));
    ASSERT(m_worldSet.size() == 1);
    ASSERT(m_normalWorld->hasOneRef());
    m_normalWorld = nullptr;
    ASSERT(m_worldSet.isEmpty());
}

void JSVMClientData::getAllWorlds(Vector<Ref<DOMWrapperWorld>>& worlds)
{
    ASSERT(worlds.isEmpty());
    
    worlds.reserveInitialCapacity(m_worldSet.size());
    for (auto it = m_worldSet.begin(), end = m_worldSet.end(); it != end; ++it)
        worlds.uncheckedAppend(*(*it));
}

void JSVMClientData::initNormalWorld(VM* vm)
{
    JSVMClientData* clientData = new JSVMClientData(*vm);
    vm->clientData = clientData; // ~VM deletes this pointer.
    
    auto constraint = std::make_unique<MarkingConstraint>(
        "Wcoc", "WebCore Output Constraints",
        [vm, clientData, lastExecutionVersion = vm->heap.mutatorExecutionVersion()]
        (SlotVisitor& slotVisitor, const VisitingTimeout&) mutable {
            Heap& heap = vm->heap;
            
            if (heap.mutatorExecutionVersion() == lastExecutionVersion)
                return;
            
            lastExecutionVersion = heap.mutatorExecutionVersion();

            // We have to manage the visit count here ourselves. We need to know that if this adds
            // opaque roots then we cannot declare termination yet. The way we signal this to the
            // constraint solver is by adding to the visit count.
            
            size_t numOpaqueRootsBefore = heap.numOpaqueRoots();

            // FIXME: Make this parallel!
            unsigned numRevisited = 0;
            clientData->forEachOutputConstraintSpace(
                [&] (Subspace& subspace) {
                    subspace.forEachMarkedCell(
                        [&] (HeapCell* heapCell, HeapCell::Kind) {
                            JSCell* cell = static_cast<JSCell*>(heapCell);
                            cell->methodTable(*vm)->visitOutputConstraints(cell, slotVisitor);
                            numRevisited++;
                        });
                });
            if (Options::logGC())
                dataLog("(", numRevisited, ")");
            
            slotVisitor.mergeIfNecessary();
            
            slotVisitor.addToVisitCount(heap.numOpaqueRoots() - numOpaqueRootsBefore);
        },
        ConstraintVolatility::SeldomGreyed);
    vm->heap.addMarkingConstraint(WTFMove(constraint));
        
    clientData->m_normalWorld = DOMWrapperWorld::create(*vm, true);
    vm->m_typedArrayController = adoptRef(new WebCoreTypedArrayController());
}

} // namespace WebCore

