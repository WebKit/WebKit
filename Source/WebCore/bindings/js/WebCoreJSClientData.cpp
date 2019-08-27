/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "DOMGCOutputConstraint.h"
#include "JSDOMBinding.h"
#include <JavaScriptCore/FastMallocAlignedMemoryAllocator.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSDestructibleObjectHeapCellType.h>
#include <JavaScriptCore/MarkingConstraint.h>
#include <JavaScriptCore/SubspaceInlines.h>
#include <JavaScriptCore/VM.h>
#include "runtime_method.h"
#include <wtf/MainThread.h>

namespace WebCore {
using namespace JSC;

JSVMClientData::JSVMClientData(VM& vm)
    : m_builtinFunctions(vm)
    , m_builtinNames(vm)
    , m_runtimeMethodSpace ISO_SUBSPACE_INIT(vm.heap, vm.destructibleObjectHeapCellType.get(), RuntimeMethod) // Hash:0xf70c4a85
    , m_outputConstraintSpace("WebCore Wrapper w/ Output Constraint", vm.heap, vm.destructibleObjectHeapCellType.get(), vm.fastMallocAllocator.get()) // Hash:0x7724c2e4
    , m_globalObjectOutputConstraintSpace("WebCore Global Object w/ Output Constraint", vm.heap, vm.cellHeapCellType.get(), vm.fastMallocAllocator.get()) // Hash:0x522d6ec9
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
    
    vm->heap.addMarkingConstraint(makeUnique<DOMGCOutputConstraint>(*vm, *clientData));
        
    clientData->m_normalWorld = DOMWrapperWorld::create(*vm, true);
    vm->m_typedArrayController = adoptRef(new WebCoreTypedArrayController());
}

} // namespace WebCore

