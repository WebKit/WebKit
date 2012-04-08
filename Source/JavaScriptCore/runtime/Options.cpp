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
#include "Options.h"

#include <limits>
#include <wtf/NumberOfCores.h>
#include <wtf/PageBlock.h>

#if OS(DARWIN) && ENABLE(PARALLEL_GC)
#include <sys/sysctl.h>
#endif

// Set to 1 to control the heuristics using environment variables.
#define ENABLE_RUN_TIME_HEURISTICS 0

#if ENABLE(RUN_TIME_HEURISTICS)
#include <stdio.h>
#include <stdlib.h>
#include <wtf/StdLibExtras.h>
#endif

namespace JSC { namespace Options {

bool useJIT;

unsigned maximumOptimizationCandidateInstructionCount;

unsigned maximumFunctionForCallInlineCandidateInstructionCount;
unsigned maximumFunctionForConstructInlineCandidateInstructionCount;

unsigned maximumInliningDepth;

int32_t thresholdForJITAfterWarmUp;
int32_t thresholdForJITSoon;

int32_t thresholdForOptimizeAfterWarmUp;
int32_t thresholdForOptimizeAfterLongWarmUp;
int32_t thresholdForOptimizeSoon;

int32_t executionCounterIncrementForLoop;
int32_t executionCounterIncrementForReturn;

unsigned desiredSpeculativeSuccessFailRatio;

double likelyToTakeSlowCaseThreshold;
double couldTakeSlowCaseThreshold;
unsigned likelyToTakeSlowCaseMinimumCount;
unsigned couldTakeSlowCaseMinimumCount;

double osrExitProminenceForFrequentExitSite;

unsigned largeFailCountThresholdBase;
unsigned largeFailCountThresholdBaseForLoop;
unsigned forcedOSRExitCountForReoptimization;

unsigned reoptimizationRetryCounterMax;
unsigned reoptimizationRetryCounterStep;

unsigned minimumOptimizationDelay;
unsigned maximumOptimizationDelay;
double desiredProfileLivenessRate;
double desiredProfileFullnessRate;

double doubleVoteRatioForDoubleFormat;

unsigned minimumNumberOfScansBetweenRebalance;
unsigned gcMarkStackSegmentSize;
unsigned minimumNumberOfCellsToKeep;
unsigned maximumNumberOfSharedSegments;
unsigned sharedStackWakeupThreshold;
unsigned numberOfGCMarkers;
unsigned opaqueRootMergeThreshold;

#if ENABLE(RUN_TIME_HEURISTICS)
static bool parse(const char* string, bool& value)
{
    if (!strcasecmp(string, "true") || !strcasecmp(string, "yes") || !strcmp(string, "1")) {
        value = true;
        return true;
    }
    if (!strcasecmp(string, "false") || !strcasecmp(string, "no") || !strcmp(string, "0")) {
        value = false;
        return true;
    }
    return false;
}

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

void initializeOptions()
{
    SET(useJIT, true);
    
    SET(maximumOptimizationCandidateInstructionCount, 10000);
    
    SET(maximumFunctionForCallInlineCandidateInstructionCount, 180);
    SET(maximumFunctionForConstructInlineCandidateInstructionCount, 100);
    
    SET(maximumInliningDepth, 5);

    SET(thresholdForJITAfterWarmUp,     100);
    SET(thresholdForJITSoon,            100);

    SET(thresholdForOptimizeAfterWarmUp,     1000);
    SET(thresholdForOptimizeAfterLongWarmUp, 5000);
    SET(thresholdForOptimizeSoon,            1000);

    SET(executionCounterIncrementForLoop,   1);
    SET(executionCounterIncrementForReturn, 15);

    SET(desiredSpeculativeSuccessFailRatio, 6);
    
    SET(likelyToTakeSlowCaseThreshold,    0.15);
    SET(couldTakeSlowCaseThreshold,       0.05); // Shouldn't be zero because some ops will spuriously take slow case, for example for linking or caching.
    SET(likelyToTakeSlowCaseMinimumCount, 100);
    SET(couldTakeSlowCaseMinimumCount,    10);
    
    SET(osrExitProminenceForFrequentExitSite, 0.3);

    SET(largeFailCountThresholdBase,         20);
    SET(largeFailCountThresholdBaseForLoop,  1);
    SET(forcedOSRExitCountForReoptimization, 250);

    SET(reoptimizationRetryCounterStep, 1);

    SET(minimumOptimizationDelay,   1);
    SET(maximumOptimizationDelay,   5);
    SET(desiredProfileLivenessRate, 0.75);
    SET(desiredProfileFullnessRate, 0.35);
    
    SET(doubleVoteRatioForDoubleFormat, 2);
    
    SET(minimumNumberOfScansBetweenRebalance, 10000);
    SET(gcMarkStackSegmentSize,               pageSize());
    SET(minimumNumberOfCellsToKeep,           10);
    SET(maximumNumberOfSharedSegments,        3);
    SET(sharedStackWakeupThreshold,           1);
    SET(opaqueRootMergeThreshold,             1000);

    int cpusToUse = 1;
#if ENABLE(PARALLEL_GC)
    cpusToUse = WTF::numberOfProcessorCores();
#endif
    // We don't scale so well beyond 4.
    if (cpusToUse > 4)
        cpusToUse = 4;
    // Be paranoid, it is the OS we're dealing with, after all.
    if (cpusToUse < 1)
        cpusToUse = 1;
    
    SET(numberOfGCMarkers, cpusToUse);

    ASSERT(thresholdForOptimizeAfterLongWarmUp >= thresholdForOptimizeAfterWarmUp);
    ASSERT(thresholdForOptimizeAfterWarmUp >= thresholdForOptimizeSoon);
    ASSERT(thresholdForOptimizeAfterWarmUp >= 0);
    
    // Compute the maximum value of the reoptimization retry counter. This is simply
    // the largest value at which we don't overflow the execute counter, when using it
    // to left-shift the execution counter by this amount. Currently the value ends
    // up being 18, so this loop is not so terrible; it probably takes up ~100 cycles
    // total on a 32-bit processor.
    reoptimizationRetryCounterMax = 0;
    while ((static_cast<int64_t>(thresholdForOptimizeAfterLongWarmUp) << (reoptimizationRetryCounterMax + 1)) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
        reoptimizationRetryCounterMax++;
    
    ASSERT((static_cast<int64_t>(thresholdForOptimizeAfterLongWarmUp) << reoptimizationRetryCounterMax) > 0);
    ASSERT((static_cast<int64_t>(thresholdForOptimizeAfterLongWarmUp) << reoptimizationRetryCounterMax) <= static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
}

} } // namespace JSC::Options


