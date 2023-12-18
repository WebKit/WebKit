/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "JITPlan.h"
#include "JITWorklistThread.h"
#include <wtf/Deque.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class CodeBlock;
class VM;

class JITWorklist {
    WTF_MAKE_NONCOPYABLE(JITWorklist);
    WTF_MAKE_TZONE_ALLOCATED(JITWorklist);

    friend class JITWorklistThread;

public:
    ~JITWorklist();

    static JITWorklist& ensureGlobalWorklist();
    static JITWorklist* existingGlobalWorklistOrNull();

    CompilationResult enqueue(Ref<JITPlan>);
    size_t queueLength() const;

    void suspendAllThreads();
    void resumeAllThreads();

    enum State { NotKnown, Compiling, Compiled };
    State compilationState(JITCompilationKey);

    State completeAllReadyPlansForVM(VM&, JITCompilationKey = JITCompilationKey());

    void waitUntilAllPlansForVMAreReady(VM&);

    // This is equivalent to:
    // worklist->waitUntilAllPlansForVMAreReady(vm);
    // worklist->completeAllReadyPlansForVM(vm);
    void completeAllPlansForVM(VM&);

    void cancelAllPlansForVM(VM&);

    void removeDeadPlans(VM&);

    unsigned setMaximumNumberOfConcurrentDFGCompilations(unsigned);
    unsigned setMaximumNumberOfConcurrentFTLCompilations(unsigned);

    // Only called on the main thread after suspending all threads.
    template<typename Visitor>
    void visitWeakReferences(Visitor&);

    template<typename Visitor>
    void iterateCodeBlocksForGC(Visitor&, VM&, const Function<void(CodeBlock*)>&);

    void dump(PrintStream&) const;

private:
    JITWorklist();

    size_t queueLength(const AbstractLocker&) const;

    template<typename MatchFunction>
    void removeMatchingPlansForVM(VM&, const MatchFunction&);

    void removeAllReadyPlansForVM(VM&, Vector<RefPtr<JITPlan>, 8>&);

    void dump(const AbstractLocker&, PrintStream&) const;

    unsigned m_numberOfActiveThreads { 0 };
    std::array<unsigned, static_cast<size_t>(JITPlan::Tier::Count)> m_ongoingCompilationsPerTier { 0, 0, 0 };
    std::array<unsigned, static_cast<size_t>(JITPlan::Tier::Count)> m_maximumNumberOfConcurrentCompilationsPerTier;

    Vector<RefPtr<JITWorklistThread>> m_threads;

    // Used to inform the thread about what work there is left to do.
    std::array<Deque<RefPtr<JITPlan>>, static_cast<size_t>(JITPlan::Tier::Count)> m_queues;

    // Used to answer questions about the current state of a code block. This
    // is particularly great for the cti_optimize OSR slow path, which wants
    // to know: did I get here because a better version of me just got
    // compiled?
    HashMap<JITCompilationKey, RefPtr<JITPlan>> m_plans;

    // Used to quickly find which plans have been compiled and are ready to
    // be completed.
    Vector<RefPtr<JITPlan>, 16> m_readyPlans;

    Lock m_suspensionLock;
    Box<Lock> m_lock;

    Ref<AutomaticThreadCondition> m_planEnqueued;
    Condition m_planCompiledOrCancelled;
};

} // namespace JSC

#endif // ENABLE(JIT)
