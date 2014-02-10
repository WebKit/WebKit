/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGWorklist.h"

#include "DeferGC.h"
#include "DFGLongLivedState.h"
#include "DFGSafepoint.h"
#include "Operations.h"
#include <mutex>

namespace JSC { namespace DFG {

Worklist::Worklist()
    : m_numberOfActiveThreads(0)
{
}

Worklist::~Worklist()
{
    {
        MutexLocker locker(m_lock);
        for (unsigned i = m_threads.size(); i--;)
            m_queue.append(nullptr); // Use null plan to indicate that we want the thread to terminate.
        m_planEnqueued.broadcast();
    }
    for (unsigned i = m_threads.size(); i--;)
        waitForThreadCompletion(m_threads[i]->m_identifier);
    ASSERT(!m_numberOfActiveThreads);
}

void Worklist::finishCreation(unsigned numberOfThreads)
{
    RELEASE_ASSERT(numberOfThreads);
    for (unsigned i = numberOfThreads; i--;) {
        std::unique_ptr<ThreadData> data = std::make_unique<ThreadData>(this);
        data->m_identifier = createThread(threadFunction, data.get(), "JSC Compilation Thread");
        m_threads.append(std::move(data));
    }
}

PassRefPtr<Worklist> Worklist::create(unsigned numberOfThreads)
{
    RefPtr<Worklist> result = adoptRef(new Worklist());
    result->finishCreation(numberOfThreads);
    return result;
}

void Worklist::enqueue(PassRefPtr<Plan> passedPlan)
{
    RefPtr<Plan> plan = passedPlan;
    MutexLocker locker(m_lock);
    if (Options::verboseCompilationQueue()) {
        dump(locker, WTF::dataFile());
        dataLog(": Enqueueing plan to optimize ", plan->key(), "\n");
    }
    ASSERT(m_plans.find(plan->key()) == m_plans.end());
    m_plans.add(plan->key(), plan);
    m_queue.append(plan);
    m_planEnqueued.signal();
}

Worklist::State Worklist::compilationState(CompilationKey key)
{
    MutexLocker locker(m_lock);
    PlanMap::iterator iter = m_plans.find(key);
    if (iter == m_plans.end())
        return NotKnown;
    return iter->value->isCompiled ? Compiled : Compiling;
}

void Worklist::waitUntilAllPlansForVMAreReady(VM& vm)
{
    DeferGC deferGC(vm.heap);
    // Wait for all of the plans for the given VM to complete. The idea here
    // is that we want all of the caller VM's plans to be done. We don't care
    // about any other VM's plans, and we won't attempt to wait on those.
    // After we release this lock, we know that although other VMs may still
    // be adding plans, our VM will not be.
    
    MutexLocker locker(m_lock);
    
    if (Options::verboseCompilationQueue()) {
        dump(locker, WTF::dataFile());
        dataLog(": Waiting for all in VM to complete.\n");
    }
    
    for (;;) {
        bool allAreCompiled = true;
        PlanMap::iterator end = m_plans.end();
        for (PlanMap::iterator iter = m_plans.begin(); iter != end; ++iter) {
            if (&iter->value->vm != &vm)
                continue;
            if (!iter->value->isCompiled) {
                allAreCompiled = false;
                break;
            }
        }
        
        if (allAreCompiled)
            break;
        
        m_planCompiled.wait(m_lock);
    }
}

void Worklist::removeAllReadyPlansForVM(VM& vm, Vector<RefPtr<Plan>, 8>& myReadyPlans)
{
    DeferGC deferGC(vm.heap);
    MutexLocker locker(m_lock);
    for (size_t i = 0; i < m_readyPlans.size(); ++i) {
        RefPtr<Plan> plan = m_readyPlans[i];
        if (&plan->vm != &vm)
            continue;
        if (!plan->isCompiled)
            continue;
        myReadyPlans.append(plan);
        m_readyPlans[i--] = m_readyPlans.last();
        m_readyPlans.removeLast();
        m_plans.remove(plan->key());
    }
}

void Worklist::removeAllReadyPlansForVM(VM& vm)
{
    Vector<RefPtr<Plan>, 8> myReadyPlans;
    removeAllReadyPlansForVM(vm, myReadyPlans);
}

Worklist::State Worklist::completeAllReadyPlansForVM(VM& vm, CompilationKey requestedKey)
{
    DeferGC deferGC(vm.heap);
    Vector<RefPtr<Plan>, 8> myReadyPlans;
    
    removeAllReadyPlansForVM(vm, myReadyPlans);
    
    State resultingState = NotKnown;

    while (!myReadyPlans.isEmpty()) {
        RefPtr<Plan> plan = myReadyPlans.takeLast();
        CompilationKey currentKey = plan->key();
        
        if (Options::verboseCompilationQueue())
            dataLog(*this, ": Completing ", currentKey, "\n");
        
        RELEASE_ASSERT(plan->isCompiled);
        
        plan->finalizeAndNotifyCallback();
        
        if (currentKey == requestedKey)
            resultingState = Compiled;
    }
    
    if (!!requestedKey && resultingState == NotKnown) {
        MutexLocker locker(m_lock);
        if (m_plans.contains(requestedKey))
            resultingState = Compiling;
    }
    
    return resultingState;
}

void Worklist::completeAllPlansForVM(VM& vm)
{
    DeferGC deferGC(vm.heap);
    waitUntilAllPlansForVMAreReady(vm);
    completeAllReadyPlansForVM(vm);
}

void Worklist::suspendAllThreads()
{
    for (unsigned i = m_threads.size(); i--;)
        m_threads[i]->m_rightToRun.lock();
}

void Worklist::resumeAllThreads()
{
    for (unsigned i = m_threads.size(); i--;)
        m_threads[i]->m_rightToRun.unlock();
}

void Worklist::visitChildren(SlotVisitor& visitor, CodeBlockSet& codeBlocks)
{
    for (PlanMap::iterator iter = m_plans.begin(); iter != m_plans.end(); ++iter) {
        iter->key.visitChildren(codeBlocks);
        iter->value->visitChildren(visitor, codeBlocks);
    }
    
    for (unsigned i = m_threads.size(); i--;) {
        ThreadData* data = m_threads[i].get();
        if (Safepoint* safepoint = data->m_safepoint)
            safepoint->visitChildren(visitor);
    }
}

size_t Worklist::queueLength()
{
    MutexLocker locker(m_lock);
    return m_queue.size();
}

void Worklist::dump(PrintStream& out) const
{
    MutexLocker locker(m_lock);
    dump(locker, out);
}

void Worklist::dump(const MutexLocker&, PrintStream& out) const
{
    out.print(
        "Worklist(", RawPointer(this), ")[Queue Length = ", m_queue.size(),
        ", Map Size = ", m_plans.size(), ", Num Ready = ", m_readyPlans.size(),
        ", Num Active Threads = ", m_numberOfActiveThreads, "/", m_threads.size(), "]");
}

void Worklist::runThread(ThreadData* data)
{
    CompilationScope compilationScope;
    
    if (Options::verboseCompilationQueue())
        dataLog(*this, ": Thread started\n");
    
    LongLivedState longLivedState;
    
    for (;;) {
        RefPtr<Plan> plan;
        {
            MutexLocker locker(m_lock);
            while (m_queue.isEmpty())
                m_planEnqueued.wait(m_lock);
            
            plan = m_queue.takeFirst();
            if (plan)
                m_numberOfActiveThreads++;
        }
        
        if (!plan) {
            if (Options::verboseCompilationQueue())
                dataLog(*this, ": Thread shutting down\n");
            return;
        }
        
        {
            MutexLocker locker(data->m_rightToRun);
        
            if (Options::verboseCompilationQueue())
                dataLog(*this, ": Compiling ", plan->key(), " asynchronously\n");
        
            plan->compileInThread(longLivedState, data);
        }
        
        {
            MutexLocker locker(m_lock);
            plan->notifyReady();
            
            if (Options::verboseCompilationQueue()) {
                dump(locker, WTF::dataFile());
                dataLog(": Compiled ", plan->key(), " asynchronously\n");
            }
            
            m_readyPlans.append(plan);
            
            m_planCompiled.broadcast();
            m_numberOfActiveThreads--;
        }
    }
}

void Worklist::threadFunction(void* argument)
{
    ThreadData* data = static_cast<ThreadData*>(argument);
    data->m_worklist->runThread(data);
}

static Worklist* theGlobalDFGWorklist;

Worklist* ensureGlobalDFGWorklist()
{
    static std::once_flag initializeGlobalWorklistOnceFlag;
    std::call_once(initializeGlobalWorklistOnceFlag, [] {
        theGlobalDFGWorklist = Worklist::create(Options::numberOfDFGCompilerThreads()).leakRef();
    });
    return theGlobalDFGWorklist;
}

Worklist* existingGlobalDFGWorklistOrNull()
{
    return theGlobalDFGWorklist;
}

static Worklist* theGlobalFTLWorklist;

Worklist* ensureGlobalFTLWorklist()
{
    static std::once_flag initializeGlobalWorklistOnceFlag;
    std::call_once(initializeGlobalWorklistOnceFlag, [] {
        theGlobalFTLWorklist = Worklist::create(Options::numberOfFTLCompilerThreads()).leakRef();
    });
    return theGlobalFTLWorklist;
}

Worklist* existingGlobalFTLWorklistOrNull()
{
    return theGlobalFTLWorklist;
}

Worklist* ensureGlobalWorklistFor(CompilationMode mode)
{
    switch (mode) {
    case InvalidCompilationMode:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    case DFGMode:
        return ensureGlobalDFGWorklist();
    case FTLMode:
    case FTLForOSREntryMode:
        return ensureGlobalFTLWorklist();
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

