/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#include "DFGCompilationKey.h"
#include "DFGCompilationMode.h"
#include "DFGDesiredGlobalProperties.h"
#include "DFGDesiredIdentifiers.h"
#include "DFGDesiredTransitions.h"
#include "DFGDesiredWatchpoints.h"
#include "DFGDesiredWeakReferences.h"
#include "DFGFinalizer.h"
#include "DeferredCompilationCallback.h"
#include "Operands.h"
#include "ProfilerCompilation.h"
#include "RecordedStatuses.h"
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class CodeBlock;
class SlotVisitor;

namespace DFG {

class ThreadData;

#if ENABLE(DFG_JIT)

class Plan : public ThreadSafeRefCounted<Plan> {
public:
    Plan(
        CodeBlock* codeBlockToCompile, CodeBlock* profiledDFGCodeBlock,
        CompilationMode, unsigned osrEntryBytecodeIndex,
        const Operands<Optional<JSValue>>& mustHandleValues);
    ~Plan();

    void compileInThread(ThreadData*);
    
    CompilationResult finalizeWithoutNotifyingCallback();
    void finalizeAndNotifyCallback();
    
    void notifyCompiling();
    void notifyReady();
    
    CompilationKey key();
    
    template<typename Func>
    void iterateCodeBlocksForGC(const Func&);
    void checkLivenessAndVisitChildren(SlotVisitor&);
    bool isKnownToBeLiveDuringGC();
    void finalizeInGC();
    void cancel();

    bool canTierUpAndOSREnter() const { return !m_tierUpAndOSREnterBytecodes.isEmpty(); }

    void cleanMustHandleValuesIfNecessary();

    VM* vm() const { return m_vm; }

    CodeBlock* codeBlock() { return m_codeBlock; }

    bool isFTL() const { return DFG::isFTL(m_mode); }
    CompilationMode mode() const { return m_mode; }
    unsigned osrEntryBytecodeIndex() const { return m_osrEntryBytecodeIndex; }
    const Operands<Optional<JSValue>>& mustHandleValues() const { return m_mustHandleValues; }
    ThreadData* threadData() const { return m_threadData; }
    Profiler::Compilation* compilation() const { return m_compilation.get(); }

    Finalizer* finalizer() const { return m_finalizer.get(); }
    void setFinalizer(std::unique_ptr<Finalizer>&& finalizer) { m_finalizer = WTFMove(finalizer); }

    RefPtr<InlineCallFrameSet> inlineCallFrames() const { return m_inlineCallFrames; }
    DesiredWatchpoints& watchpoints() { return m_watchpoints; }
    DesiredIdentifiers& identifiers() { return m_identifiers; }
    DesiredWeakReferences& weakReferences() { return m_weakReferences; }
    DesiredTransitions& transitions() { return m_transitions; }
    DesiredGlobalProperties& globalProperties() { return m_globalProperties; }
    RecordedStatuses& recordedStatuses() { return m_recordedStatuses; }

    bool willTryToTierUp() const { return m_willTryToTierUp; }
    void setWillTryToTierUp(bool willTryToTierUp) { m_willTryToTierUp = willTryToTierUp; }

    HashMap<unsigned, Vector<unsigned>>& tierUpInLoopHierarchy() { return m_tierUpInLoopHierarchy; }
    Vector<unsigned>& tierUpAndOSREnterBytecodes() { return m_tierUpAndOSREnterBytecodes; }

    enum Stage { Preparing, Compiling, Ready, Cancelled };
    Stage stage() const { return m_stage; }

    DeferredCompilationCallback* callback() const { return m_callback.get(); }
    void setCallback(Ref<DeferredCompilationCallback>&& callback) { m_callback = WTFMove(callback); }

private:
    bool computeCompileTimes() const;
    bool reportCompileTimes() const;
    
    enum CompilationPath { FailPath, DFGPath, FTLPath, CancelPath };
    CompilationPath compileInThreadImpl();
    
    bool isStillValidOnMainThread();
    bool isStillValid();
    void reallyAdd(CommonData*);

    // Warning: pretty much all of the pointer fields in this object get nulled by cancel(). So, if
    // you're writing code that is callable on the cancel path, be sure to null check everything!

    VM* m_vm;

    // These can be raw pointers because we visit them during every GC in checkLivenessAndVisitChildren.
    CodeBlock* m_codeBlock;
    CodeBlock* m_profiledDFGCodeBlock;

    CompilationMode m_mode;
    const unsigned m_osrEntryBytecodeIndex;
    Operands<Optional<JSValue>> m_mustHandleValues;
    bool m_mustHandleValuesMayIncludeGarbage { true };
    Lock m_mustHandleValueCleaningLock;

    ThreadData* m_threadData;

    RefPtr<Profiler::Compilation> m_compilation;

    std::unique_ptr<Finalizer> m_finalizer;

    RefPtr<InlineCallFrameSet> m_inlineCallFrames;
    DesiredWatchpoints m_watchpoints;
    DesiredIdentifiers m_identifiers;
    DesiredWeakReferences m_weakReferences;
    DesiredTransitions m_transitions;
    DesiredGlobalProperties m_globalProperties;
    RecordedStatuses m_recordedStatuses;

    bool m_willTryToTierUp { false };

    HashMap<unsigned, Vector<unsigned>> m_tierUpInLoopHierarchy;
    Vector<unsigned> m_tierUpAndOSREnterBytecodes;

    Stage m_stage;

    RefPtr<DeferredCompilationCallback> m_callback;

    MonotonicTime m_timeBeforeFTL;
};

#endif // ENABLE(DFG_JIT)

} } // namespace JSC::DFG
