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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmCallee.h"
#include "WasmCodeBlock.h"
#include "WasmModuleInformation.h"
#include "WasmPlan.h"
#include "WasmWorklist.h"
#include <wtf/Expected.h>
#include <wtf/Lock.h>
#include <wtf/SharedTask.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {
    
class Module : public ThreadSafeRefCounted<Module> {
public:
    using ValidationResult = WTF::Expected<RefPtr<Module>, String>;
    typedef void CallbackType(VM&, ValidationResult&&);
    using AsyncValidationCallback = RefPtr<SharedTask<CallbackType>>;

    static Ref<Module> create(Ref<ModuleInformation>&& moduleInformation)
    {
        return adoptRef(*new Module(WTFMove(moduleInformation)));
    }

    static ValidationResult validateSync(VM& vm, Vector<uint8_t>&& source)
    {
        Ref<Plan> plan = adoptRef(*new Plan(vm, WTFMove(source), Plan::Validation, Plan::dontFinalize()));
        return validateSyncImpl(WTFMove(plan));
    }

    static void validateAsync(VM& vm, Vector<uint8_t>&& source, AsyncValidationCallback&& callback)
    {
        Ref<Plan> plan = adoptRef(*new Plan(vm, WTFMove(source), Plan::Validation, makeValidationCallback(WTFMove(callback))));
        Wasm::ensureWorklist().enqueue(WTFMove(plan));
    }

    Wasm::SignatureIndex signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
    {
        return m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
    }

    const Wasm::ModuleInformation& moduleInformation() const { return m_moduleInformation.get(); }

    Ref<CodeBlock> compileSync(VM&, MemoryMode);
    void compileAsync(VM&, MemoryMode, CodeBlock::AsyncCompilationCallback&&);

    Ref<CodeBlock> nonNullCodeBlock(Wasm::MemoryMode mode)
    {
        CodeBlock* codeBlock = m_codeBlocks[static_cast<uint8_t>(mode)].get();
        RELEASE_ASSERT(!!codeBlock);
        return makeRef(*codeBlock);
    }

private:
    Ref<CodeBlock> getOrCreateCodeBlock(VM&, MemoryMode);

    static ValidationResult validateSyncImpl(Ref<Plan>&&);
    static Plan::CompletionTask makeValidationCallback(AsyncValidationCallback&&);

    Module(Ref<ModuleInformation>&&);
    Ref<ModuleInformation> m_moduleInformation;
    RefPtr<CodeBlock> m_codeBlocks[Wasm::NumberOfMemoryModes];
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
