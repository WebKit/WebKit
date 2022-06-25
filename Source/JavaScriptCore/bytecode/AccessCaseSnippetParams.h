/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "SnippetParams.h"

namespace JSC {

struct AccessGenerationState;

class AccessCaseSnippetParams final : public SnippetParams {
public:
    AccessCaseSnippetParams(VM& vm, Vector<Value>&& regs, Vector<GPRReg>&& gpScratch, Vector<FPRReg>&& fpScratch)
        : SnippetParams(vm, WTFMove(regs), WTFMove(gpScratch), WTFMove(fpScratch))
    {
    }

    class SlowPathCallGenerator {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~SlowPathCallGenerator() { }
        virtual CCallHelpers::JumpList generate(AccessGenerationState&, const RegisterSet& usedRegistersBySnippet, CCallHelpers&) = 0;
    };

    CCallHelpers::JumpList emitSlowPathCalls(AccessGenerationState&, const RegisterSet& usedRegistersBySnippet, CCallHelpers&);

private:
#define JSC_DEFINE_CALL_OPERATIONS(OperationType, ResultType, ...) void addSlowPathCallImpl(CCallHelpers::JumpList, CCallHelpers&, OperationType, ResultType, std::tuple<__VA_ARGS__> args) final;
    SNIPPET_SLOW_PATH_CALLS(JSC_DEFINE_CALL_OPERATIONS)
#undef JSC_DEFINE_CALL_OPERATIONS
    Vector<std::unique_ptr<SlowPathCallGenerator>> m_generators;
};

}

#endif
