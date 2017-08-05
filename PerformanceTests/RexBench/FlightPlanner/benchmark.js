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
"use strict";

let currentTime;
if (this.performance && performance.now)
    currentTime = function() { return performance.now() };
else if (this.preciseTime)
    currentTime = function() { return preciseTime() * 1000; };
else
    currentTime = function() { return +new Date(); };

class FlightPlannerBenchmark {
    constructor(verbose = 0)
    {
        this._verbose = verbose;
    }

    runIteration()
    {
        for (let iteration = 0; iteration < 10; ++iteration) {
            setupUserWaypoints();
            
            for (let flightPlan of expectedFlightPlans) {
                flightPlan.reset();
                flightPlan.resolveRoute();
            }
        }
        this.checkResults();
    }

    checkResults()
    {
        for (let flightPlan of expectedFlightPlans) {
            flightPlan.calculate();
            flightPlan.checkExpectations();
        }
    }
}

function runBenchmark()
{
    const verbose = 0;
    const numIterations = 10;

    let benchmark = new FlightPlannerBenchmark(verbose);

    let before = currentTime();

    for (let iteration = 0; iteration < numIterations; ++iteration)
        benchmark.runIteration();
    
    let after = currentTime();

    for (let iteration = 0; iteration < numIterations; ++iteration)
        benchmark.checkResults();
    
    return after - before;
}


//  print("Benchmark ran in " + runBenchmark());
