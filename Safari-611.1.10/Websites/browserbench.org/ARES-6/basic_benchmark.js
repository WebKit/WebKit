/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

const BasicBenchmarkCode = String.raw`
<script src="Basic/ast.js"></script>
<script src="Basic/basic.js"></script>
<script src="Basic/caseless_map.js"></script>
<script src="Basic/lexer.js"></script>
<script src="Basic/number.js"></script>
<script src="Basic/parser.js"></script>
<script src="Basic/random.js"></script>
<script src="Basic/state.js"></script>
<script src="Basic/util.js"></script>
<script src="Basic/benchmark.js"></script>
<script>
var results = [];
var benchmark = new BasicBenchmark();
var numIterations = 200;
for (var i = 0; i < numIterations; ++i) {
    var before = currentTime();
    benchmark.runIteration();
    var after = currentTime();
    results.push(after - before);
}
reportResult(results);
</script>`;


let runBasicBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "Basic/ast.js"
        , "Basic/basic.js"
        , "Basic/caseless_map.js"
        , "Basic/lexer.js"
        , "Basic/number.js"
        , "Basic/parser.js"
        , "Basic/random.js"
        , "Basic/state.js"
        , "Basic/util.js"
        , "Basic/benchmark.js"
    ];

    runBasicBenchmark = makeBenchmarkRunner(sources, "BasicBenchmark");
}

const BasicBenchmarkRunner = {
    name: "Basic",
    code: BasicBenchmarkCode,
    run: runBasicBenchmark,
    cells: {}
};

if (isInBrowser) {
    BasicBenchmarkRunner.cells = {
        firstIteration: document.getElementById("BasicFirstIteration"),
        averageWorstCase: document.getElementById("BasicAverageWorstCase"),
        steadyState: document.getElementById("BasicSteadyState"),
        message: document.getElementById("BasicMessage")
    };
}
