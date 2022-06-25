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

const AirBenchmarkCode = String.raw`
<script src="Air/symbols.js"></script>
<script src="Air/tmp_base.js"></script>
<script src="Air/arg.js"></script>
<script src="Air/basic_block.js"></script>
<script src="Air/code.js"></script>
<script src="Air/frequented_block.js"></script>
<script src="Air/inst.js"></script>
<script src="Air/opcode.js"></script>
<script src="Air/reg.js"></script>
<script src="Air/stack_slot.js"></script>
<script src="Air/tmp.js"></script>
<script src="Air/util.js"></script>
<script src="Air/custom.js"></script>
<script src="Air/liveness.js"></script>
<script src="Air/insertion_set.js"></script>
<script src="Air/allocate_stack.js"></script>
<script src="Air/payload-gbemu-executeIteration.js"></script>
<script src="Air/payload-imaging-gaussian-blur-gaussianBlur.js"></script>
<script src="Air/payload-airjs-ACLj8C.js"></script>
<script src="Air/payload-typescript-scanIdentifier.js"></script>
<script src="Air/benchmark.js"></script>
<script>
var results = [];
var benchmark = new AirBenchmark();
var numIterations = 200;
for (var i = 0; i < numIterations; ++i) {
    var before = currentTime();
    benchmark.runIteration();
    var after = currentTime();
    results.push(after - before);
}
reportResult(results);
</script>`;

let runAirBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "Air/symbols.js"
        , "Air/tmp_base.js"
        , "Air/arg.js"
        , "Air/basic_block.js"
        , "Air/code.js"
        , "Air/frequented_block.js"
        , "Air/inst.js"
        , "Air/opcode.js"
        , "Air/reg.js"
        , "Air/stack_slot.js"
        , "Air/tmp.js"
        , "Air/util.js"
        , "Air/custom.js"
        , "Air/liveness.js"
        , "Air/insertion_set.js"
        , "Air/allocate_stack.js"
        , "Air/payload-gbemu-executeIteration.js"
        , "Air/payload-imaging-gaussian-blur-gaussianBlur.js"
        , "Air/payload-airjs-ACLj8C.js"
        , "Air/payload-typescript-scanIdentifier.js"
        , "Air/benchmark.js"
    ];

    runAirBenchmark = makeBenchmarkRunner(sources, "AirBenchmark");
}

const AirBenchmarkRunner = {
    code: AirBenchmarkCode,
    run: runAirBenchmark,
    cells: { },
    name: "Air"
};

if (isInBrowser) {
    AirBenchmarkRunner.cells = {
        firstIteration: document.getElementById("AirFirstIteration"),
        averageWorstCase: document.getElementById("AirAverageWorstCase"),
        steadyState: document.getElementById("AirSteadyState"),
        message: document.getElementById("AirMessage")
    }
}
