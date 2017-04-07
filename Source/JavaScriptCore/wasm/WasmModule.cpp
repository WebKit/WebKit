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
#include "WasmModule.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmPlanInlines.h"

namespace JSC { namespace Wasm {

Module::Module(Ref<ModuleInformation>&& moduleInformation)
    : m_moduleInformation(WTFMove(moduleInformation))
{
}

static Module::ValidationResult makeValidationResult(Plan& plan)
{
    ASSERT(!plan.hasWork());
    if (plan.failed())
        return UnexpectedType<String>(plan.errorMessage());
    return Module::ValidationResult(Module::create(plan.takeModuleInformation()));
}

Module::ValidationResult Module::validateSyncImpl(Ref<Plan>&& plan)
{
    plan->parseAndValidateModule();
    return makeValidationResult(plan.get());
}

Ref<CodeBlock> Module::getOrCreateCodeBlock(VM& vm, MemoryMode mode)
{
    RefPtr<CodeBlock> codeBlock;
    auto locker = holdLock(m_lock);
    codeBlock = m_codeBlocks[static_cast<uint8_t>(mode)];
    // If a previous attempt at a compile errored out, let's try again.
    // Compilations from valid modules can fail because OOM and cancellation.
    // It's worth retrying.
    // FIXME: We might want to back off retrying at some point:
    // https://bugs.webkit.org/show_bug.cgi?id=170607
    if (!codeBlock || !codeBlock->runnable()) {
        codeBlock = CodeBlock::create(vm, mode, const_cast<ModuleInformation&>(moduleInformation()));
        m_codeBlocks[static_cast<uint8_t>(mode)] = codeBlock;
    }
    return codeBlock.releaseNonNull();
}

Ref<CodeBlock> Module::compileSync(VM& vm, MemoryMode mode)
{
    Ref<CodeBlock> codeBlock = getOrCreateCodeBlock(vm, mode);
    codeBlock->waitUntilFinished();
    return codeBlock;
}

Plan::CompletionTask Module::makeValidationCallback(AsyncValidationCallback&& callback)
{
    return createSharedTask<Plan::CallbackType>([callback = WTFMove(callback)] (VM& vm, Plan& plan) {
        ASSERT(!plan.hasWork());
        callback->run(vm, makeValidationResult(plan));
    });
}

void Module::compileAsync(VM& vm, MemoryMode mode, CodeBlock::AsyncCompilationCallback&& task)
{
    Ref<CodeBlock> codeBlock = getOrCreateCodeBlock(vm, mode);
    codeBlock->compileAsync(vm, WTFMove(task));
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
