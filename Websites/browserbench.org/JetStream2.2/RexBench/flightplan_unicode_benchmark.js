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

const FlightPlannerUnicodeBenchmarkCode = String.raw`
<script src="benchmark.js"></script>
<script src="FlightPlanner/airways.js"></script>
<script src="FlightPlanner/waypoints.js"></script>
<script src="FlightPlanner/use_unicode.js"></script>
<script src="FlightPlanner/flight_planner.js"></script>
<script src="FlightPlanner/expectations.js"></script>
<script src="FlightPlanner/benchmark.js"></script>
<script>
"use strict";
var results = [];
var benchmark = new FlightPlannerBenchmark();
var numIterations = 200;
benchmark.runIterations(numIterations, results);
reportResult(results);
</script>`;


let runFlightPlannerUnicodeBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "benchmark.js"
        , "FlightPlanner/airways.js"
        , "FlightPlanner/waypoints.js"
        , "FlightPlanner/use_unicode.js"
        , "FlightPlanner/flight_planner.js"
        , "FlightPlanner/expectations.js"
        , "FlightPlanner/benchmark.js"
    ];

    runFlightPlannerUnicodeBenchmark = makeBenchmarkRunner(sources, "FlightPlannerBenchmark", 200);
}

const FlightPlannerUnicodeBenchmarkRunner = {
    name: "Flight Planner - Unicode",
    code: FlightPlannerUnicodeBenchmarkCode,
    run: runFlightPlannerUnicodeBenchmark,
    cells: {}
};

if (isInBrowser) {
    FlightPlannerUnicodeBenchmarkRunner.cells = {
        firstIteration: document.getElementById("FlightPlannerUnicodeFirstIteration"),
        averageWorstCase: document.getElementById("FlightPlannerUnicodeAverageWorstCase"),
        steadyState: document.getElementById("FlightPlannerUnicodeSteadyState"),
        message: document.getElementById("FlightPlannerUnicodeMessage")
    };
}
