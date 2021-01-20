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

#pragma once

#include "VisitCounter.h"
#include <wtf/BitVector.h>
#include <wtf/Condition.h>
#include <wtf/Deque.h>
#include <wtf/FastMalloc.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/ScopedLambda.h>
#include <wtf/Vector.h>

namespace JSC {

class Heap;
class MarkingConstraint;
class MarkingConstraintSet;

class MarkingConstraintSolver {
    WTF_MAKE_NONCOPYABLE(MarkingConstraintSolver);
    WTF_MAKE_FAST_ALLOCATED;
    
public:
    MarkingConstraintSolver(MarkingConstraintSet&);
    ~MarkingConstraintSolver();
    
    bool didVisitSomething() const;
    
    enum SchedulerPreference {
        ParallelWorkFirst,
        NextConstraintFirst
    };

    void execute(SchedulerPreference, ScopedLambda<Optional<unsigned>()> pickNext);
    
    void drain(BitVector& unexecuted);
    
    void converge(const Vector<MarkingConstraint*>& order);
    
    void execute(MarkingConstraint&);
    
    // Parallel constraints can add parallel tasks.
    void addParallelTask(RefPtr<SharedTask<void(SlotVisitor&)>>, MarkingConstraint&);
    
private:
    void runExecutionThread(SlotVisitor&, SchedulerPreference, ScopedLambda<Optional<unsigned>()> pickNext);
    
    struct TaskWithConstraint {
        TaskWithConstraint() { }
        
        TaskWithConstraint(RefPtr<SharedTask<void(SlotVisitor&)>> task, MarkingConstraint* constraint)
            : task(WTFMove(task))
            , constraint(constraint)
        {
        }
        
        bool operator==(const TaskWithConstraint& other) const
        {
            return task == other.task
                && constraint == other.constraint;
        }
        
        RefPtr<SharedTask<void(SlotVisitor&)>> task;
        MarkingConstraint* constraint { nullptr };
    };
    
    Heap& m_heap;
    SlotVisitor& m_mainVisitor;
    MarkingConstraintSet& m_set;
    BitVector m_executed;
    Deque<TaskWithConstraint, 32> m_toExecuteInParallel;
    Vector<unsigned, 32> m_toExecuteSequentially;
    Lock m_lock;
    Condition m_condition;
    bool m_pickNextIsStillActive { true };
    unsigned m_numThreadsThatMayProduceWork { 0 };
    Vector<VisitCounter, 16> m_visitCounters;
};

} // namespace JSC

