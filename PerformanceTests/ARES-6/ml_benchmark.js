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

const MLBenchmarkCode = String.raw`
<script src="ml/index.js"></script>
<script src="ml/benchmark.js"></script>
<script>
var results = [];
var benchmark = new MLBenchmark();
var numIterations = 60;
for (var i = 0; i < numIterations; ++i) {
    var before = currentTime();
    benchmark.runIteration();
    var after = currentTime();
    results.push(after - before);
}
reportResult(results);
</script>`;


let runMLBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "ml/index.js"
        , "ml/benchmark.js"
    ];

    runMLBenchmark = makeBenchmarkRunner(sources, "MLBenchmark", 60);
}

const MLBenchmarkRunner = {
    name: "ML",
    code: MLBenchmarkCode,
    run: runMLBenchmark,
    cells: {}
};

if (isInBrowser) {
    MLBenchmarkRunner.cells = {
        firstIteration: document.getElementById("MLFirstIteration"),
        averageWorstCase: document.getElementById("MLAverageWorstCase"),
        steadyState: document.getElementById("MLSteadyState"),
        message: document.getElementById("MLMessage")
    };
}
