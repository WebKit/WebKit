/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "BaselineJITPlan.h"

#if ENABLE(JIT)

namespace JSC {

BaselineJITPlan::BaselineJITPlan(CodeBlock* codeBlock, BytecodeIndex loopOSREntryBytecodeIndex)
    : JITPlan(JITCompilationMode::Baseline, codeBlock)
    , m_jit(codeBlock->vm(), codeBlock, loopOSREntryBytecodeIndex)
{
#if CPU(ARM64E)
    m_jit.m_assembler.buffer().arm64eHash().deallocatePinForCurrentThread();
#endif
    m_jit.doMainThreadPreparationBeforeCompile();
}

auto BaselineJITPlan::compileInThreadImpl() -> CompilationPath
{
#if CPU(ARM64E)
    m_jit.m_assembler.buffer().arm64eHash().allocatePinForCurrentThreadAndInitializeHash();
#endif
    m_jit.compileAndLinkWithoutFinalizing(JITCompilationCanFail);
    return BaselinePath;
}

size_t BaselineJITPlan::codeSize() const
{
    return m_jit.codeSize();
}

CompilationResult BaselineJITPlan::finalize()
{
    CompilationResult result = m_jit.finalizeOnMainThread(m_codeBlock);
    switch (result) {
    case CompilationFailed:
        CODEBLOCK_LOG_EVENT(m_codeBlock, "delayJITCompile", ("compilation failed"));
        dataLogLnIf(Options::verboseOSR(), "    JIT compilation failed.");
        m_codeBlock->dontJITAnytimeSoon();
        m_codeBlock->m_didFailJITCompilation = true;
        break;
    case CompilationSuccessful:
        WTF::crossModifyingCodeFence();
        dataLogLnIf(Options::verboseOSR(), "    JIT compilation successful.");
        m_codeBlock->ownerExecutable()->installCode(m_codeBlock);
        m_codeBlock->jitSoon();
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    return result;
}

} // namespace JSC

#endif // ENABLE(JIT)
