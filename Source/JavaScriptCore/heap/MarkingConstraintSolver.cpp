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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MarkingConstraintSolver.h"

#include "JSCInlines.h"

namespace JSC { 

MarkingConstraintSolver::MarkingConstraintSolver(MarkingConstraintSet& set)
    : m_heap(set.m_heap)
    , m_mainVisitor(m_heap.collectorSlotVisitor())
    , m_set(set)
{
    m_heap.forEachSlotVisitor(
        [&] (SlotVisitor& visitor) {
            m_visitCounters.append(VisitCounter(visitor));
        });
}

MarkingConstraintSolver::~MarkingConstraintSolver()
{
}

bool MarkingConstraintSolver::didVisitSomething() const
{
    for (const VisitCounter& visitCounter : m_visitCounters) {
        if (visitCounter.visitCount())
            return true;
    }
    // If the number of SlotVisitors increases after creating m_visitCounters,
    // we conservatively say there could be something visited by added SlotVisitors.
    if (m_heap.numberOfSlotVisitors() > m_visitCounters.size())
        return true;
    return false;
}

void MarkingConstraintSolver::execute(SchedulerPreference preference, ScopedLambda<Optional<unsigned>()> pickNext)
{
    m_pickNextIsStillActive = true;
    RELEASE_ASSERT(!m_numThreadsThatMayProduceWork);
    
    if (Options::useParallelMarkingConstraintSolver()) {
        if (Options::logGC())
            dataLog(preference == ParallelWorkFirst ? "P" : "N", "<");
        
        m_heap.runFunctionInParallel(
            [&] (SlotVisitor& visitor) { runExecutionThread(visitor, preference, pickNext); });
        
        if (Options::logGC())
            dataLog(">");
    } else
        runExecutionThread(m_mainVisitor, preference, pickNext);
    
    RELEASE_ASSERT(!m_pickNextIsStillActive);
    RELEASE_ASSERT(!m_numThreadsThatMayProduceWork);
        
    if (!m_toExecuteSequentially.isEmpty()) {
        for (unsigned indexToRun : m_toExecuteSequentially)
            execute(*m_set.m_set[indexToRun]);
        m_toExecuteSequentially.clear();
    }
        
    RELEASE_ASSERT(m_toExecuteInParallel.isEmpty());
}

void MarkingConstraintSolver::drain(BitVector& unexecuted)
{
    auto iter = unexecuted.begin();
    auto end = unexecuted.end();
    if (iter == end)
        return;
    auto pickNext = scopedLambda<Optional<unsigned>()>(
        [&] () -> Optional<unsigned> {
            if (iter == end)
                return WTF::nullopt;
            return *iter++;
        });
    execute(NextConstraintFirst, pickNext);
    unexecuted.clearAll();
}

void MarkingConstraintSolver::converge(const Vector<MarkingConstraint*>& order)
{
    if (didVisitSomething())
        return;
    
    if (order.isEmpty())
        return;
        
    size_t index = 0;

    // We want to execute the first constraint sequentially if we think it will quickly give us a
    // result. If we ran it in parallel to other constraints, then we might end up having to wait for
    // those other constraints to finish, which would be a waste of time since during convergence it's
    // empirically most optimal to return to draining as soon as a constraint generates work. Most
    // constraints don't generate any work most of the time, and when they do generate work, they tend
    // to generate enough of it to feed a decent draining cycle. Therefore, pause times are lowest if
    // we get the heck out of here as soon as a constraint generates work. I think that part of what
    // makes this optimal is that we also never abort running a constraint early, so when we do run
    // one, it has an opportunity to generate as much work as it possibly can.
    if (order[index]->quickWorkEstimate(m_mainVisitor) > 0.) {
        execute(*order[index++]);
        
        if (m_toExecuteInParallel.isEmpty()
            && (order.isEmpty() || didVisitSomething()))
            return;
    }
    
    auto pickNext = scopedLambda<Optional<unsigned>()>(
        [&] () -> Optional<unsigned> {
            if (didVisitSomething())
                return WTF::nullopt;
            
            if (index >= order.size())
                return WTF::nullopt;
            
            MarkingConstraint& constraint = *order[index++];
            return constraint.index();
        });
    
    execute(ParallelWorkFirst, pickNext);
}

void MarkingConstraintSolver::execute(MarkingConstraint& constraint)
{
    if (m_executed.get(constraint.index()))
        return;
    
    constraint.prepareToExecute(NoLockingNecessary, m_mainVisitor);
    constraint.execute(m_mainVisitor);
    m_executed.set(constraint.index());
}

void MarkingConstraintSolver::addParallelTask(RefPtr<SharedTask<void(SlotVisitor&)>> task, MarkingConstraint& constraint)
{
    auto locker = holdLock(m_lock);
    m_toExecuteInParallel.append(TaskWithConstraint(WTFMove(task), &constraint));
}

void MarkingConstraintSolver::runExecutionThread(SlotVisitor& visitor, SchedulerPreference preference, ScopedLambda<Optional<unsigned>()> pickNext)
{
    for (;;) {
        bool doParallelWorkMode;
        MarkingConstraint* constraint = nullptr;
        unsigned indexToRun = UINT_MAX;
        TaskWithConstraint task;
        {
            auto locker = holdLock(m_lock);
                        
            for (;;) {
                auto tryParallelWork = [&] () -> bool {
                    if (m_toExecuteInParallel.isEmpty())
                        return false;
                    
                    task = m_toExecuteInParallel.first();
                    constraint = task.constraint;
                    doParallelWorkMode = true;
                    return true;
                };
                
                auto tryNextConstraint = [&] () -> bool {
                    if (!m_pickNextIsStillActive)
                        return false;
                    
                    for (;;) {
                        Optional<unsigned> pickResult = pickNext();
                        if (!pickResult) {
                            m_pickNextIsStillActive = false;
                            return false;
                        }
                        
                        if (m_executed.get(*pickResult))
                            continue;
                                    
                        MarkingConstraint& candidateConstraint = *m_set.m_set[*pickResult];
                        if (candidateConstraint.concurrency() == ConstraintConcurrency::Sequential) {
                            m_toExecuteSequentially.append(*pickResult);
                            continue;
                        }
                        if (candidateConstraint.parallelism() == ConstraintParallelism::Parallel)
                            m_numThreadsThatMayProduceWork++;
                        indexToRun = *pickResult;
                        constraint = &candidateConstraint;
                        doParallelWorkMode = false;
                        constraint->prepareToExecute(locker, visitor);
                        return true;
                    }
                };
                
                if (preference == ParallelWorkFirst) {
                    if (tryParallelWork() || tryNextConstraint())
                        break;
                } else {
                    if (tryNextConstraint() || tryParallelWork())
                        break;
                }
                
                // This means that we have nothing left to run. The only way for us to have more work is
                // if someone is running a constraint that may produce parallel work.
                
                if (!m_numThreadsThatMayProduceWork)
                    return;
                
                // FIXME: Any waiting could be replaced with just running the SlotVisitor.
                // I wonder if that would be profitable.
                m_condition.wait(m_lock);
            }
        }
                    
        if (doParallelWorkMode)
            constraint->doParallelWork(visitor, *task.task);
        else {
            if (constraint->parallelism() == ConstraintParallelism::Parallel) {
                visitor.m_currentConstraint = constraint;
                visitor.m_currentSolver = this;
            }
            
            constraint->execute(visitor);
            
            visitor.m_currentConstraint = nullptr;
            visitor.m_currentSolver = nullptr;
        }
        
        {
            auto locker = holdLock(m_lock);
            
            if (doParallelWorkMode) {
                if (!m_toExecuteInParallel.isEmpty()
                    && task == m_toExecuteInParallel.first())
                    m_toExecuteInParallel.takeFirst();
                else
                    ASSERT(!m_toExecuteInParallel.contains(task));
            } else {
                if (constraint->parallelism() == ConstraintParallelism::Parallel)
                    m_numThreadsThatMayProduceWork--;
                m_executed.set(indexToRun);
            }
                        
            m_condition.notifyAll();
        }
    }
}

} // namespace JSC

