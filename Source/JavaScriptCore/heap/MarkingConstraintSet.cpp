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
#include "MarkingConstraintSet.h"

#include "JSCInlines.h"
#include "MarkingConstraintSolver.h"
#include "Options.h"
#include "SimpleMarkingConstraint.h"
#include "SuperSampler.h"
#include <wtf/Function.h>
#include <wtf/TimeWithDynamicClockType.h>

namespace JSC {

MarkingConstraintSet::MarkingConstraintSet(Heap& heap)
    : m_heap(heap)
{
}

MarkingConstraintSet::~MarkingConstraintSet()
{
}

void MarkingConstraintSet::didStartMarking()
{
    m_unexecutedRoots.clearAll();
    m_unexecutedOutgrowths.clearAll();
    for (auto& constraint : m_set) {
        constraint->resetStats();
        switch (constraint->volatility()) {
        case ConstraintVolatility::GreyedByExecution:
            m_unexecutedRoots.set(constraint->index());
            break;
        case ConstraintVolatility::GreyedByMarking:
            m_unexecutedOutgrowths.set(constraint->index());
            break;
        case ConstraintVolatility::SeldomGreyed:
            break;
        }
    }
    m_iteration = 1;
}

void MarkingConstraintSet::add(CString abbreviatedName, CString name, ::Function<void(SlotVisitor&)> function, ConstraintVolatility volatility, ConstraintConcurrency concurrency, ConstraintParallelism parallelism)
{
    add(makeUnique<SimpleMarkingConstraint>(WTFMove(abbreviatedName), WTFMove(name), WTFMove(function), volatility, concurrency, parallelism));
}

void MarkingConstraintSet::add(
    std::unique_ptr<MarkingConstraint> constraint)
{
    constraint->m_index = m_set.size();
    m_ordered.append(constraint.get());
    if (constraint->volatility() == ConstraintVolatility::GreyedByMarking)
        m_outgrowths.append(constraint.get());
    m_set.append(WTFMove(constraint));
}

bool MarkingConstraintSet::executeConvergence(SlotVisitor& visitor)
{
    bool result = executeConvergenceImpl(visitor);
    dataLogIf(Options::logGC(), " ");
    return result;
}

bool MarkingConstraintSet::isWavefrontAdvancing(SlotVisitor& visitor)
{
    for (MarkingConstraint* outgrowth : m_outgrowths) {
        if (outgrowth->workEstimate(visitor))
            return true;
    }
    return false;
}

bool MarkingConstraintSet::executeConvergenceImpl(SlotVisitor& visitor)
{
    SuperSamplerScope superSamplerScope(false);
    MarkingConstraintSolver solver(*this);
    
    unsigned iteration = m_iteration++;
    
    dataLogIf(Options::logGC(), "i#", iteration, ":");

    if (iteration == 1) {
        // First iteration is before any visitor draining, so it's unlikely to trigger any constraints
        // other than roots.
        solver.drain(m_unexecutedRoots);
        return false;
    }
    
    if (iteration == 2) {
        solver.drain(m_unexecutedOutgrowths);
        return false;
    }
    
    // We want to keep preferring the outgrowth constraints - the ones that need to be fixpointed
    // even in a stop-the-world GC - until they stop producing. They have a tendency to go totally
    // silent at some point during GC, at which point it makes sense not to run them again until
    // the end. Outgrowths producing new information corresponds almost exactly to the wavefront
    // advancing: it usually means that we are marking objects that should be marked based on
    // other objects that we would have marked anyway. Once the wavefront is no longer advancing,
    // we want to run mostly the root constraints (based on their predictions of how much work
    // they will have) because at this point we are just trying to outpace the retreating
    // wavefront.
    //
    // Note that this function (executeConvergenceImpl) only returns true if it runs all
    // constraints. So, all we are controlling are the heuristics that say which constraints to
    // run first. Choosing the constraints that are the most likely to produce means running fewer
    // constraints before returning.
    bool isWavefrontAdvancing = this->isWavefrontAdvancing(visitor);
    
    std::sort(
        m_ordered.begin(), m_ordered.end(),
        [&] (MarkingConstraint* a, MarkingConstraint* b) -> bool {
            // Remember: return true if a should come before b.
            
            auto volatilityScore = [] (MarkingConstraint* constraint) -> unsigned {
                return constraint->volatility() == ConstraintVolatility::GreyedByMarking ? 1 : 0;
            };
            
            unsigned aVolatilityScore = volatilityScore(a);
            unsigned bVolatilityScore = volatilityScore(b);
            
            if (aVolatilityScore != bVolatilityScore) {
                if (isWavefrontAdvancing)
                    return aVolatilityScore > bVolatilityScore;
                else
                    return aVolatilityScore < bVolatilityScore;
            }
            
            double aWorkEstimate = a->workEstimate(visitor);
            double bWorkEstimate = b->workEstimate(visitor);
            
            if (aWorkEstimate != bWorkEstimate)
                return aWorkEstimate > bWorkEstimate;
            
            // This causes us to use SeldomGreyed vs GreyedByExecution as a final tie-breaker.
            return a->volatility() > b->volatility();
        });
    
    solver.converge(m_ordered);
    
    // Return true if we've converged. That happens if we did not visit anything.
    return !solver.didVisitSomething();
}

void MarkingConstraintSet::executeAll(SlotVisitor& visitor)
{
    for (auto& constraint : m_set)
        constraint->execute(visitor);
    dataLogIf(Options::logGC(), " ");
}

} // namespace JSC

