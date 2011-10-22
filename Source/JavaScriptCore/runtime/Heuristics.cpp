/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "Heuristics.h"

#include <limits>

// Set to 1 to control the heuristics using environment variables.
#define ENABLE_RUN_TIME_HEURISTICS 0

#if ENABLE(RUN_TIME_HEURISTICS)
#include <stdio.h>
#include <stdlib.h>
#include <wtf/StdLibExtras.h>
#endif

namespace JSC { namespace Heuristics {

unsigned maximumEvalOptimizationCandidateInstructionCount;
unsigned maximumProgramOptimizationCandidateInstructionCount;
unsigned maximumFunctionForCallOptimizationCandidateInstructionCount;
unsigned maximumFunctionForConstructOptimizationCandidateInstructionCount;

unsigned maximumFunctionForCallInlineCandidateInstructionCount;

unsigned maximumInliningDepth;

int32_t executionCounterValueForOptimizeAfterWarmUp;
int32_t executionCounterValueForOptimizeAfterLongWarmUp;
int32_t executionCounterValueForDontOptimizeAnytimeSoon;
int32_t executionCounterValueForOptimizeSoon;
int32_t executionCounterValueForOptimizeNextInvocation;

int32_t executionCounterIncrementForLoop;
int32_t executionCounterIncrementForReturn;

unsigned desiredSpeculativeSuccessFailRatio;

unsigned likelyToTakeSlowCaseThreshold;
unsigned couldTakeSlowCaseThreshold;

unsigned largeFailCountThresholdBase;
unsigned largeFailCountThresholdBaseForLoop;

unsigned reoptimizationRetryCounterMax;
unsigned reoptimizationRetryCounterStep;

unsigned maximumOptimizationDelay;
double desiredProfileLivenessRate;
double desiredProfileFullnessRate;

#if ENABLE(RUN_TIME_HEURISTICS)
static bool parse(const char* string, int32_t& value)
{
    return sscanf(string, "%d", &value) == 1;
}

static bool parse(const char* string, unsigned& value)
{
    return sscanf(string, "%u", &value) == 1;
}

static bool parse(const char* string, double& value)
{
    return sscanf(string, "%lf", &value) == 1;
}

template<typename T, typename U>
void setHeuristic(T& variable, const char* name, U value)
{
    const char* stringValue = getenv(name);
    if (!stringValue) {
        variable = safeCast<T>(value);
        return;
    }
    
    if (parse(stringValue, variable))
        return;
    
    fprintf(stderr, "WARNING: failed to parse %s=%s\n", name, stringValue);
    variable = safeCast<T>(value);
}

#define SET(variable, value) setHeuristic(variable, "JSC_" #variable, value)
#else
#define SET(variable, value) variable = value
#endif

void initializeHeuristics()
{
    // FIXME: Must revisit these heuristics! The DFG, being an optimizing compiler, may
    // take a long time for pathologically huge code blocks. The best way to cope with
    // this is to refuse to optimize them.
    SET(maximumEvalOptimizationCandidateInstructionCount,                 std::numeric_limits<unsigned>::max());
    SET(maximumProgramOptimizationCandidateInstructionCount,              std::numeric_limits<unsigned>::max());
    SET(maximumFunctionForCallOptimizationCandidateInstructionCount,      std::numeric_limits<unsigned>::max());
    SET(maximumFunctionForConstructOptimizationCandidateInstructionCount, std::numeric_limits<unsigned>::max());
    
    SET(maximumFunctionForCallInlineCandidateInstructionCount, 100);
    
    SET(maximumInliningDepth, 3);

    SET(executionCounterValueForOptimizeAfterWarmUp,     -1000);
    SET(executionCounterValueForOptimizeAfterLongWarmUp, -5000);
    SET(executionCounterValueForDontOptimizeAnytimeSoon, std::numeric_limits<int32_t>::min());
    SET(executionCounterValueForOptimizeSoon,            -1000);
    SET(executionCounterValueForOptimizeNextInvocation,  0);

    SET(executionCounterIncrementForLoop,   1);
    SET(executionCounterIncrementForReturn, 15);

    SET(desiredSpeculativeSuccessFailRatio, 6);
    
    SET(likelyToTakeSlowCaseThreshold, 100);
    SET(couldTakeSlowCaseThreshold,    10); // Shouldn't be zero because some ops will spuriously take slow case, for example for linking or caching.

    SET(largeFailCountThresholdBase,        20);
    SET(largeFailCountThresholdBaseForLoop, 1);

    SET(reoptimizationRetryCounterStep, 1);

    SET(maximumOptimizationDelay,   5);
    SET(desiredProfileLivenessRate, 0.75);
    SET(desiredProfileFullnessRate, 0.25);
    
    ASSERT(executionCounterValueForDontOptimizeAnytimeSoon <= executionCounterValueForOptimizeAfterLongWarmUp);
    ASSERT(executionCounterValueForOptimizeAfterLongWarmUp <= executionCounterValueForOptimizeAfterWarmUp);
    ASSERT(executionCounterValueForOptimizeAfterWarmUp <= executionCounterValueForOptimizeSoon);
    ASSERT(executionCounterValueForOptimizeAfterWarmUp < 0);
    ASSERT(executionCounterValueForOptimizeSoon <= executionCounterValueForOptimizeNextInvocation);
    
    // Compute the maximum value of the reoptimization retry counter. This is simply
    // the largest value at which we don't overflow the execute counter, when using it
    // to left-shift the execution counter by this amount. Currently the value ends
    // up being 18, so this loop is not so terrible; it probably takes up ~100 cycles
    // total on a 32-bit processor.
    reoptimizationRetryCounterMax = 0;
    while ((static_cast<int64_t>(executionCounterValueForOptimizeAfterLongWarmUp) << (reoptimizationRetryCounterMax + 1)) >= static_cast<int64_t>(std::numeric_limits<int32_t>::min()))
        reoptimizationRetryCounterMax++;
    
    ASSERT((static_cast<int64_t>(executionCounterValueForOptimizeAfterLongWarmUp) << reoptimizationRetryCounterMax) < 0);
    ASSERT((static_cast<int64_t>(executionCounterValueForOptimizeAfterLongWarmUp) << reoptimizationRetryCounterMax) >= static_cast<int64_t>(std::numeric_limits<int32_t>::min()));
}

} } // namespace JSC::Heuristics


