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

#ifndef DFGWorklist_h
#define DFGWorklist_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "DFGPlan.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/ThreadingPrimitives.h>

namespace JSC { namespace DFG {

class Worklist : public RefCounted<Worklist> {
public:
    enum State { NotKnown, Compiling, Compiled };

    ~Worklist();
    
    static PassRefPtr<Worklist> create(unsigned numberOfThreads);
    
    void enqueue(PassRefPtr<Plan>);
    
    // This is equivalent to:
    // worklist->waitUntilAllPlansForVMAreReady(vm);
    // worklist->completeAllReadyPlansForVM(vm);
    void completeAllPlansForVM(VM&);
    
    void waitUntilAllPlansForVMAreReady(VM&);
    State completeAllReadyPlansForVM(VM&, CompilationKey = CompilationKey());
    void removeAllReadyPlansForVM(VM&);
    
    State compilationState(CompilationKey);
    
    size_t queueLength();
    void dump(PrintStream&) const;
    
private:
    Worklist();
    void finishCreation(unsigned numberOfThreads);
    
    void runThread();
    static void threadFunction(void* argument);
    
    void removeAllReadyPlansForVM(VM&, Vector<RefPtr<Plan>, 8>&);

    void dump(const MutexLocker&, PrintStream&) const;

    // Used to inform the thread about what work there is left to do.
    Deque<RefPtr<Plan>, 16> m_queue;
    
    // Used to answer questions about the current state of a code block. This
    // is particularly great for the cti_optimize OSR slow path, which wants
    // to know: did I get here because a better version of me just got
    // compiled?
    typedef HashMap<CompilationKey, RefPtr<Plan>> PlanMap;
    PlanMap m_plans;
    
    // Used to quickly find which plans have been compiled and are ready to
    // be completed.
    Vector<RefPtr<Plan>, 16> m_readyPlans;
    
    mutable Mutex m_lock;
    ThreadCondition m_planEnqueued;
    ThreadCondition m_planCompiled;
    Vector<ThreadIdentifier> m_threads;
    unsigned m_numberOfActiveThreads;
};

// For DFGMode compilations.
Worklist* ensureGlobalDFGWorklist();
Worklist* existingGlobalDFGWorklistOrNull();

// For FTLMode and FTLForOSREntryMode compilations.
Worklist* ensureGlobalFTLWorklist();
Worklist* existingGlobalFTLWorklistOrNull();

Worklist* ensureGlobalWorklistFor(CompilationMode);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGWorklist_h

