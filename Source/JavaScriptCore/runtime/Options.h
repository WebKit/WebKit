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

#ifndef Options_h
#define Options_h

#include <stdint.h>

namespace JSC { namespace Options {

// maximumInliningDepth is the maximum depth of inline stack, so 1 = no
// inlining, 2 = one level, etc

// couldTakeSlowCaseThreshold shouldn't be zero because some ops will spuriously
// take slow case, for example for linking or caching.

#define FOR_EACH_HEURISTIC(m) \
    m(unsigned, maximumOptimizationCandidateInstructionCount, 1000) \
    \
    m(unsigned, maximumFunctionForCallInlineCandidateInstructionCount, 150) \
    m(unsigned, maximumFunctionForConstructInlineCandidateInstructionCount, 80) \
    \
    m(unsigned, maximumInliningDepth, 5)                              \
    \
    m(int32_t, executionCounterValueForOptimizeAfterWarmUp,      -1000) \
    m(int32_t, executionCounterValueForOptimizeAfterLongWarmUp,  -5000) \
    m(int32_t, executionCounterValueForDontOptimizeAnytimeSoon,  std::numeric_limits<int32_t>::min()) \
    m(int32_t, executionCounterValueForOptimizeSoon,             -1000) \
    m(int32_t, executionCounterValueForOptimizeNextInvocation,   0) \
    \
    m(int32_t, executionCounterIncrementForLoop,       1) \
    m(int32_t, executionCounterIncrementForReturn,     15) \
    \
    m(unsigned, desiredSpeculativeSuccessFailRatio,    6) \
    \
    m(double,   likelyToTakeSlowCaseThreshold,         0.15) \
    m(double,   couldTakeSlowCaseThreshold,            0.05) \
    m(unsigned, likelyToTakeSlowCaseMinimumCount,      100) \
    m(unsigned, couldTakeSlowCaseMinimumCount,         10) \
    \
    m(double,   osrExitProminenceForFrequentExitSite,  0.3) \
    \
    m(unsigned, largeFailCountThresholdBase,           20) \
    m(unsigned, largeFailCountThresholdBaseForLoop,    1) \
    \
    m(unsigned, reoptimizationRetryCounterMax,         0) \
    m(unsigned, reoptimizationRetryCounterStep,        1) \
    \
    m(unsigned, minimumOptimizationDelay,              1) \
    m(unsigned, maximumOptimizationDelay,              5) \
    m(double, desiredProfileLivenessRate,              0.75) \
    m(double, desiredProfileFullnessRate,              0.35) \
    \
    m(double,   doubleVoteRatioForDoubleFormat,        2) \
    \
    m(unsigned, minimumNumberOfScansBetweenRebalance,  10000) \
    m(unsigned, gcMarkStackSegmentSize,                0) \
    m(unsigned, minimumNumberOfCellsToKeep,            10) \
    m(unsigned, maximumNumberOfSharedSegments,         3) \
    m(unsigned, sharedStackWakeupThreshold,            1) \
    m(unsigned, numberOfGCMarkers,                     0) \
    m(unsigned, opaqueRootMergeThreshold,              1000)

#define FOR_EACH_OPTION(m) \
    FOR_EACH_HEURISTIC(m)

#define DECLARE(type, cname, default_val) extern type cname;
FOR_EACH_OPTION(DECLARE)
#undef DECLARE

void initializeOptions();

} } // namespace JSC::Options

#endif // Options_h

