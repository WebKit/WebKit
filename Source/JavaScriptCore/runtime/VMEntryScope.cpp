/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "VM.h"
#include <wtf/StackBounds.h>

namespace JSC {

VMEntryScope::VMEntryScope(VM& vm, JSGlobalObject* globalObject)
    : m_vm(vm)
    , m_stack(wtfThreadData().stack())
    , m_globalObject(globalObject)
    , m_prev(vm.entryScope)
    , m_prevStackLimit(vm.stackLimit())
    , m_recompilationNeeded(false)
{
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
    // Clear the exception stack between entries
    vm.clearExceptionStack();

    void* limit = m_stack.recursionLimit(requiredCapacity());
    vm.setStackLimit(limit);
}

VMEntryScope::~VMEntryScope()
{
    m_vm.entryScope = m_prev;
    m_vm.setStackLimit(m_prevStackLimit);

    if (m_recompilationNeeded) {
        if (m_vm.entryScope)
            m_vm.entryScope->setRecompilationNeeded(true);
        else {
            if (Debugger* debugger = m_globalObject->debugger())
                debugger->recompileAllJSFunctions(&m_vm);
        }
    }
}

size_t VMEntryScope::requiredCapacity() const
{
    Interpreter* interpreter = m_vm.interpreter;

    // We require a smaller stack budget for the error stack. This is to allow
    // some minimal JS execution to proceed and do the work of throwing a stack
    // overflow error if needed. In contrast, arbitrary JS code will require the
    // more generous stack budget in order to proceed.
    //
    // These sizes were derived from the stack usage of a number of sites when
    // layout occurs when we've already consumed most of the C stack.
    const size_t requiredStack = 128 * KB;
    const size_t errorModeRequiredStack = 64 * KB;

    size_t requiredCapacity = interpreter->isInErrorHandlingMode() ? errorModeRequiredStack : requiredStack;
    RELEASE_ASSERT(m_stack.size() >= requiredCapacity);
    return requiredCapacity;
}

} // namespace JSC
