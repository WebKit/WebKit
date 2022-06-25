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

const Octane2RegExpBenchmarkCode = String.raw`
<script src="benchmark.js"></script>
<script src="Octane2/regexp.js"></script>
<script src="Octane2/benchmark.js"></script>
<script>
"use script";      
var results = [];
var numIterations = 100;
var benchmark = new Octane2RegExpBenchmark();
benchmark.runIterations(numIterations, results);
reportResult(results);
</script>`;


let runOctane2RegExpBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "benchmark.js"
        , "Octane2/regexp.js"
        , "Octane2/benchmark.js"
    ];

    runOctane2RegExpBenchmark = makeBenchmarkRunner(sources, "Octane2RegExpBenchmark", 100);
}

const Octane2RegExpBenchmarkRunner = {
    name: "Octane2 RegExp",
    code: Octane2RegExpBenchmarkCode,
    run: runOctane2RegExpBenchmark,
    cells: {}
};

if (isInBrowser) {
    Octane2RegExpBenchmarkRunner.cells = {
        firstIteration: document.getElementById("Octane2RegExpFirstIteration"),
        averageWorstCase: document.getElementById("Octane2RegExpAverageWorstCase"),
        steadyState: document.getElementById("Octane2RegExpSteadyState"),
        message: document.getElementById("Octane2RegExpMessage")
    };
}
