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

#include "JITSafepoint.h"

#if ENABLE(JIT)

namespace JSC {

BaselineJITPlan::BaselineJITPlan(CodeBlock* codeBlock)
    : JITPlan(JITCompilationMode::Baseline, codeBlock)
{
    JIT::doMainThreadPreparationBeforeCompile(codeBlock->vm());
}

auto BaselineJITPlan::compileInThreadImpl(JITCompilationEffort effort) -> CompilationPath
{
    // BaselineJITPlan can keep underlying CodeBlock alive while running.
    // So we do not need to suspend this compilation thread while running GC.
    Safepoint::Result result;
    {
        Safepoint safepoint(*this, result);
        safepoint.begin(false);

        JIT jit(*m_vm, *this, m_codeBlock);
        auto jitCode = jit.compileAndLinkWithoutFinalizing(effort);
        m_jitCode = WTFMove(jitCode);
    }
    if (result.didGetCancelled())
        return CancelPath;
    return BaselinePath;
}

auto BaselineJITPlan::compileInThreadImpl() -> CompilationPath
{
    return compileInThreadImpl(JITCompilationCanFail);
}

auto BaselineJITPlan::compileSync(JITCompilationEffort effort) -> CompilationPath
{
    return compileInThreadImpl(effort);
}

size_t BaselineJITPlan::codeSize() const
{
    if (m_jitCode)
        return m_jitCode->size();
    return 0;
}

bool BaselineJITPlan::isKnownToBeLiveAfterGC()
{
    // If stage is not JITPlanStage::Canceled, we should keep this alive and mark underlying CodeBlock anyway.
    // Regardless of whether the owner ScriptExecutable / CodeBlock dies, compiled code would be still usable
    // since Baseline JIT is *unlinked*. So, let's not stop compilation.
    return m_stage != JITPlanStage::Canceled;
}

bool BaselineJITPlan::isKnownToBeLiveDuringGC(AbstractSlotVisitor&)
{
    // Ditto to isKnownToBeLiveAfterGC. Unless plan gets completely cancelled before running, we should keep compilation running.
    return m_stage != JITPlanStage::Canceled;
}

CompilationResult BaselineJITPlan::finalize()
{
    CompilationResult result = JIT::finalizeOnMainThread(m_codeBlock, *this, m_jitCode);
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
