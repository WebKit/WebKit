/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "MarkingConstraint.h"

#include "JSCInlines.h"
#include "VisitCounter.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

static constexpr bool verboseMarkingConstraint = false;

WTF_MAKE_TZONE_ALLOCATED_IMPL(MarkingConstraint);

MarkingConstraint::MarkingConstraint(CString abbreviatedName, CString name, ConstraintVolatility volatility, ConstraintConcurrency concurrency, ConstraintParallelism parallelism)
    : m_abbreviatedName(abbreviatedName)
    , m_name(WTFMove(name))
    , m_volatility(volatility)
    , m_concurrency(concurrency)
    , m_parallelism(parallelism)
{
}

MarkingConstraint::~MarkingConstraint()
{
}

void MarkingConstraint::resetStats()
{
    m_lastVisitCount = 0;
}

void MarkingConstraint::execute(SlotVisitor& visitor)
{
    ASSERT(!visitor.heap()->isMarkingForGCVerifier());
    VisitCounter visitCounter(visitor);
    executeImpl(visitor);
    m_lastVisitCount += visitCounter.visitCount();
    if (verboseMarkingConstraint && visitCounter.visitCount())
        dataLog("(", abbreviatedName(), " visited ", visitCounter.visitCount(), " in execute)");
}

void MarkingConstraint::executeSynchronously(AbstractSlotVisitor& visitor)
{
    prepareToExecuteImpl(NoLockingNecessary, visitor);
    executeImpl(visitor);
}

double MarkingConstraint::quickWorkEstimate(SlotVisitor&)
{
    return 0;
}

double MarkingConstraint::workEstimate(SlotVisitor& visitor)
{
    return lastVisitCount() + quickWorkEstimate(visitor);
}

void MarkingConstraint::prepareToExecute(const AbstractLocker& constraintSolvingLocker, SlotVisitor& visitor)
{
    ASSERT(!visitor.heap()->isMarkingForGCVerifier());
    dataLogIf(Options::logGC(), abbreviatedName());
    VisitCounter visitCounter(visitor);
    prepareToExecuteImpl(constraintSolvingLocker, visitor);
    m_lastVisitCount = visitCounter.visitCount();
    if (verboseMarkingConstraint && visitCounter.visitCount())
        dataLog("(", abbreviatedName(), " visited ", visitCounter.visitCount(), " in prepareToExecute)");
}

void MarkingConstraint::doParallelWork(SlotVisitor& visitor, SharedTask<void(SlotVisitor&)>& task)
{
    ASSERT(!visitor.heap()->isMarkingForGCVerifier());
    VisitCounter visitCounter(visitor);
    task.run(visitor);
    if (verboseMarkingConstraint && visitCounter.visitCount())
        dataLog("(", abbreviatedName(), " visited ", visitCounter.visitCount(), " in doParallelWork)");
    {
        Locker locker { m_lock };
        m_lastVisitCount += visitCounter.visitCount();
    }
}

void MarkingConstraint::prepareToExecuteImpl(const AbstractLocker&, AbstractSlotVisitor&)
{
}

} // namespace JSC

