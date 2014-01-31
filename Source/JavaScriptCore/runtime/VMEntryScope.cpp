/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "VMEntryScope.h"

#include "Debugger.h"
#include "Options.h"
#include "VM.h"
#include <wtf/StackBounds.h>

namespace JSC {

VMEntryScope::VMEntryScope(VM& vm, JSGlobalObject* globalObject)
    : m_vm(vm)
    , m_globalObject(globalObject)
    , m_recompilationNeeded(false)
{
    ASSERT(wtfThreadData().stack().isGrowingDownward());
    if (!vm.entryScope) {
#if ENABLE(ASSEMBLER)
        if (ExecutableAllocator::underMemoryPressure())
            vm.heap.deleteAllCompiledCode();
#endif
        vm.entryScope = this;

        // Reset the date cache between JS invocations to force the VM to
        // observe time xone changes.
        vm.resetDateCache();
    }

    // Clear the captured exception stack between entries
    vm.clearExceptionStack();
}

VMEntryScope::~VMEntryScope()
{
    if (m_vm.entryScope != this)
        return;

    m_vm.entryScope = nullptr;

    if (m_recompilationNeeded) {
        if (Debugger* debugger = m_globalObject->debugger())
            debugger->recompileAllJSFunctions(&m_vm);
    }
}

} // namespace JSC
