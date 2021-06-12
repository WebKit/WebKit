/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "CompilationResult.h"
#include "DFGDesiredGlobalProperties.h"
#include "DFGDesiredIdentifiers.h"
#include "DFGDesiredTransitions.h"
#include "DFGDesiredWatchpoints.h"
#include "DFGDesiredWeakReferences.h"
#include "DFGFinalizer.h"
#include "DeferredCompilationCallback.h"
#include "JITPlan.h"
#include "Operands.h"
#include "ProfilerCompilation.h"
#include "RecordedStatuses.h"
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class CodeBlock;

namespace DFG {

class ThreadData;

#if ENABLE(DFG_JIT)

class Plan final : public JITPlan {
    using Base = JITPlan;

public:
    Plan(
        CodeBlock* codeBlockToCompile, CodeBlock* profiledDFGCodeBlock,
        JITCompilationMode, BytecodeIndex osrEntryBytecodeIndex,
        const Operands<std::optional<JSValue>>& mustHandleValues);
    ~Plan();

    size_t codeSize() const final;
    void finalizeInGC() final;
    CompilationResult finalize() override;

    void notifyReady() final;
    void cancel() final;

    bool isKnownToBeLiveAfterGC() final;
    bool isKnownToBeLiveDuringGC(AbstractSlotVisitor&) final;
    bool iterateCodeBlocksForGC(AbstractSlotVisitor&, const Function<void(CodeBlock*)>&) final;
    bool checkLivenessAndVisitChildren(AbstractSlotVisitor&) final;


    bool canTierUpAndOSREnter() const { return !m_tierUpAndOSREnterBytecodes.isEmpty(); }

    void cleanMustHandleValuesIfNecessary();

    BytecodeIndex osrEntryBytecodeIndex() const { return m_osrEntryBytecodeIndex; }
    const Operands<std::optional<JSValue>>& mustHandleValues() const { return m_mustHandleValues; }
    Profiler::Compilation* compilation() const { return m_compilation.get(); }

    Finalizer* finalizer() const { return m_finalizer.get(); }
    void setFinalizer(std::unique_ptr<Finalizer>&& finalizer) { m_finalizer = WTFMove(finalizer); }

    RefPtr<InlineCallFrameSet> inlineCallFrames() const { return m_inlineCallFrames; }
    DesiredWatchpoints& watchpoints() { return m_watchpoints; }
    DesiredIdentifiers& identifiers() { return m_identifiers; }
    DesiredWeakReferences& weakReferences() { return m_weakReferences; }
    DesiredTransitions& transitions() { return m_transitions; }
    RecordedStatuses& recordedStatuses() { return m_recordedStatuses; }

    bool willTryToTierUp() const { return m_willTryToTierUp; }
    void setWillTryToTierUp(bool willTryToTierUp) { m_willTryToTierUp = willTryToTierUp; }

    HashMap<BytecodeIndex, FixedVector<BytecodeIndex>>& tierUpInLoopHierarchy() { return m_tierUpInLoopHierarchy; }
    Vector<BytecodeIndex>& tierUpAndOSREnterBytecodes() { return m_tierUpAndOSREnterBytecodes; }

    DeferredCompilationCallback* callback() const { return m_callback.get(); }
    void setCallback(Ref<DeferredCompilationCallback>&& callback) { m_callback = WTFMove(callback); }

private:
    CompilationPath compileInThreadImpl() override;
    
    bool isStillValidOnMainThread();
    bool isStillValid();
    void reallyAdd(CommonData*);

    // These can be raw pointers because we visit them during every GC in checkLivenessAndVisitChildren.
    CodeBlock* m_profiledDFGCodeBlock;

    Operands<std::optional<JSValue>> m_mustHandleValues;
    bool m_mustHandleValuesMayIncludeGarbage WTF_GUARDED_BY_LOCK(m_mustHandleValueCleaningLock) { true };
    Lock m_mustHandleValueCleaningLock;

    bool m_willTryToTierUp { false };

    const BytecodeIndex m_osrEntryBytecodeIndex;

    RefPtr<Profiler::Compilation> m_compilation;

    std::unique_ptr<Finalizer> m_finalizer;

    RefPtr<InlineCallFrameSet> m_inlineCallFrames;
    DesiredWatchpoints m_watchpoints;
    DesiredIdentifiers m_identifiers;
    DesiredWeakReferences m_weakReferences;
    DesiredTransitions m_transitions;
    RecordedStatuses m_recordedStatuses;

    HashMap<BytecodeIndex, FixedVector<BytecodeIndex>> m_tierUpInLoopHierarchy;
    Vector<BytecodeIndex> m_tierUpAndOSREnterBytecodes;

    RefPtr<DeferredCompilationCallback> m_callback;
};

#endif // ENABLE(DFG_JIT)

} } // namespace JSC::DFG
