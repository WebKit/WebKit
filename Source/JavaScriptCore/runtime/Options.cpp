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

#define DEFINE(type, cname, default_val) type cname;
FOR_EACH_OPTION(DEFINE)
#undef DEFINE

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

void initializeOptions()
{
#define INIT(type, cname, default_val) SET(cname, default_val);
    FOR_EACH_OPTION(INIT)
#undef INIT

    // Now we initialize heuristics whose defaults are not known at
    // compile-time.

    if (!gcMarkStackSegmentSize)
        gcMarkStackSegmentSize = pageSize();

    if (!numberOfGCMarkers) {
        int cpusToUse = 1;
#if OS(DARWIN) && ENABLE(PARALLEL_GC)
        int name[2];
        size_t valueSize = sizeof(cpusToUse);
        name[0] = CTL_HW;
        name[1] = HW_AVAILCPU;
        sysctl(name, 2, &cpusToUse, &valueSize, 0, 0);
#endif
        // We don't scale so well beyond 4.
        if (cpusToUse > 4)
            cpusToUse = 4;
        // Be paranoid, it is the OS we're dealing with, after all.
        if (cpusToUse < 1)
            cpusToUse = 1;

        numberOfGCMarkers = cpusToUse;
    }
    
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

} } // namespace JSC::Options


