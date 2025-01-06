/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "JITPlan.h"

#if ENABLE(JIT)

#include "AbstractSlotVisitor.h"
#include "CodeBlock.h"
#include "HeapInlines.h"
#include "JITSafepoint.h"
#include "JITWorklistThread.h"
#include "JSCellInlines.h"
#include "VMInlines.h"
#include <wtf/CompilationThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/SystemTracing.h>

namespace JSC {

extern Seconds totalBaselineCompileTime;
extern Seconds totalDFGCompileTime;
extern Seconds totalFTLCompileTime;
extern Seconds totalFTLDFGCompileTime;
extern Seconds totalFTLB3CompileTime;

JITPlan::JITPlan(JITCompilationMode mode, CodeBlock* codeBlock)
    : m_mode(mode)
    , m_vm(&codeBlock->vm())
    , m_codeBlock(codeBlock)
{
    m_vm->changeNumberOfActiveJITPlans(1);
}

JITPlan::~JITPlan()
{
    if (m_vm)
        m_vm->changeNumberOfActiveJITPlans(-1);
}

void JITPlan::cancel()
{
    RELEASE_ASSERT(m_stage != JITPlanStage::Canceled);
    RELEASE_ASSERT(!safepointKeepsDependenciesLive());
    ASSERT(m_vm);
    m_vm->changeNumberOfActiveJITPlans(-1);
    m_stage = JITPlanStage::Canceled;
    m_vm = nullptr;
    m_codeBlock = nullptr;
}

void JITPlan::notifyCompiling()
{
    m_stage = JITPlanStage::Compiling;
}

void JITPlan::notifyReady()
{
    m_stage = JITPlanStage::Ready;
}

auto JITPlan::tier() const -> Tier
{
    switch (m_mode) {
    case JITCompilationMode::InvalidCompilation:
        RELEASE_ASSERT_NOT_REACHED();
        return Tier::Baseline;
    case JITCompilationMode::Baseline:
        return Tier::Baseline;
    case JITCompilationMode::DFG:
    case JITCompilationMode::UnlinkedDFG:
        return Tier::DFG;
    case JITCompilationMode::FTL:
    case JITCompilationMode::FTLForOSREntry:
        return Tier::FTL;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

JITCompilationKey JITPlan::key()
{
    JSCell* codeBlock;
    if (m_mode == JITCompilationMode::Baseline)
        codeBlock = m_codeBlock->unlinkedCodeBlock();
    else
        codeBlock = m_codeBlock->baselineAlternative();
    return JITCompilationKey(codeBlock, m_mode);
}

bool JITPlan::isKnownToBeLiveAfterGC()
{
    if (m_stage == JITPlanStage::Canceled)
        return false;
    if (!m_vm->heap.isMarked(m_codeBlock->ownerExecutable()))
        return false;
    return true;
}

bool JITPlan::isKnownToBeLiveDuringGC(AbstractSlotVisitor& visitor)
{
    if (m_stage == JITPlanStage::Canceled)
        return false;
    if (!visitor.isMarked(m_codeBlock->ownerExecutable()))
        return false;
    return true;
}

bool JITPlan::iterateCodeBlocksForGC(AbstractSlotVisitor& visitor, const Function<void(CodeBlock*)>& func)
{
    if (!isKnownToBeLiveDuringGC(visitor))
        return false;

    // Compilation writes lots of values to a CodeBlock without performing
    // an explicit barrier. So, we need to be pessimistic and assume that
    // all our CodeBlocks must be visited during GC.
    func(m_codeBlock);
    return true;
}

bool JITPlan::checkLivenessAndVisitChildren(AbstractSlotVisitor& visitor)
{
    if (!isKnownToBeLiveDuringGC(visitor))
        return false;

    visitor.appendUnbarriered(m_codeBlock);
    return true;
}

bool JITPlan::isInSafepoint() const
{
    return m_thread && m_thread->safepoint();
}

bool JITPlan::safepointKeepsDependenciesLive() const
{
    return m_thread && m_thread->safepoint() && m_thread->safepoint()->keepDependenciesLive();
}

bool JITPlan::computeCompileTimes() const
{
    return reportCompileTimes()
        || Options::reportTotalCompileTimes()
        || (m_vm && m_vm->m_perBytecodeProfiler);
}

bool JITPlan::reportCompileTimes() const
{
    return Options::reportCompileTimes()
        || (Options::reportBaselineCompileTimes() && m_mode == JITCompilationMode::Baseline)
        || (Options::reportDFGCompileTimes() && isDFG())
        || (Options::reportFTLCompileTimes() && isFTL());
}

void JITPlan::compileInThread(JITWorklistThread* thread)
{
    SetForScope threadScope(m_thread, thread);

    MonotonicTime before;
    CString codeBlockName;

    bool computeCompileTimes = this->computeCompileTimes();
    if (UNLIKELY(computeCompileTimes)) {
        before = MonotonicTime::now();
        if (reportCompileTimes())
            codeBlockName = toCString(*m_codeBlock);
    }

    CompilationScope compilationScope;

#if ENABLE(DFG_JIT)
    if (UNLIKELY(DFG::logCompilationChanges(m_mode) || Options::logPhaseTimes()))
        dataLog("DFG(Plan) compiling ", *m_codeBlock, " with ", m_mode, ", instructions size = ", m_codeBlock->instructionsSize(), "\n");
#endif // ENABLE(DFG_JIT)

    CString signpostMessage;
    UNUSED_VARIABLE(signpostMessage);
    if (UNLIKELY(Options::useCompilerSignpost())) {
        StringPrintStream stream;
        stream.print(m_mode, " ", *m_codeBlock, " instructions size = ", m_codeBlock->instructionsSize());
        signpostMessage = stream.toCString();
        WTFBeginSignpost(this, JSCJITCompiler, "%" PUBLIC_LOG_STRING, signpostMessage.data() ? signpostMessage.data() : "(nullptr)");
    }

    CompilationPath path = compileInThreadImpl();

    RELEASE_ASSERT((path == CancelPath) == (m_stage == JITPlanStage::Canceled));

    if (UNLIKELY(Options::useCompilerSignpost()))
        WTFEndSignpost(this, JSCJITCompiler, "%" PUBLIC_LOG_STRING, signpostMessage.data() ? signpostMessage.data() : "(nullptr)");

    if (LIKELY(!computeCompileTimes))
        return;

    MonotonicTime after = MonotonicTime::now();

    if (Options::reportTotalCompileTimes()) {
        if (isFTL()) {
            totalFTLCompileTime += after - before;
            totalFTLDFGCompileTime += m_timeBeforeFTL - before;
            totalFTLB3CompileTime += after - m_timeBeforeFTL;
        } else if (mode() == JITCompilationMode::Baseline)
            totalBaselineCompileTime += after - before;
        else
            totalDFGCompileTime += after - before;
    }

    const char* pathName = nullptr;
    switch (path) {
    case FailPath:
        pathName = "N/A (fail)";
        break;
    case BaselinePath:
        pathName = "Baseline";
        break;
    case DFGPath:
        pathName = "DFG";
        break;
    case FTLPath:
        pathName = "FTL";
        break;
    case CancelPath:
        pathName = "Canceled";
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    if (m_codeBlock) { // m_codeBlock will be null if the compilation was cancelled.
        switch (path) {
        case FTLPath:
            CODEBLOCK_LOG_EVENT(m_codeBlock, "ftlCompile", ("took ", (after - before).milliseconds(), " ms (DFG: ", (m_timeBeforeFTL - before).milliseconds(), ", B3: ", (after - m_timeBeforeFTL).milliseconds(), ") with ", pathName));
            break;
        case DFGPath:
            CODEBLOCK_LOG_EVENT(m_codeBlock, "dfgCompile", ("took ", (after - before).milliseconds(), " ms with ", pathName));
            break;
        case BaselinePath:
            CODEBLOCK_LOG_EVENT(m_codeBlock, "baselineCompile", ("took ", (after - before).milliseconds(), " ms with ", pathName));
            break;
        case FailPath:
            CODEBLOCK_LOG_EVENT(m_codeBlock, "failed compilation", ("took ", (after - before).milliseconds(), " ms with ", pathName));
            break;
        case CancelPath:
            CODEBLOCK_LOG_EVENT(m_codeBlock, "cancelled compilation", ("took ", (after - before).milliseconds(), " ms with ", pathName));
            break;
        }
    }
    if (UNLIKELY(reportCompileTimes())) {
        dataLog("Optimized ", codeBlockName, " using ", m_mode, " with ", pathName, " into ", codeSize(), " bytes in ", (after - before).milliseconds(), " ms");
        if (path == FTLPath)
            dataLog(" (DFG: ", (m_timeBeforeFTL - before).milliseconds(), ", B3: ", (after - m_timeBeforeFTL).milliseconds(), ")");
        dataLog(".\n");
    }
}

void JITPlan::runMainThreadFinalizationTasks()
{
    auto tasks = std::exchange(m_mainThreadFinalizationTasks, { });
    for (auto& task : tasks)
        task->run();
}

} // namespace JSC

#endif // ENABLE(DFG_JIT)
