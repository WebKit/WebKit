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

const OfflineAssemblerBenchmarkCode = String.raw`
<script src="benchmark.js"></script>
<script src="OfflineAssembler/registers.js"></script>
<script src="OfflineAssembler/instructions.js"></script>
<script src="OfflineAssembler/ast.js"></script>
<script src="OfflineAssembler/parser.js"></script>
<script src="OfflineAssembler/file.js"></script>
<script src="OfflineAssembler/LowLevelInterpreter.js"></script>
<script src="OfflineAssembler/LowLevelInterpreter32_64.js"></script>
<script src="OfflineAssembler/LowLevelInterpreter64.js"></script>
<script src="OfflineAssembler/InitBytecodes.js"></script>
<script src="OfflineAssembler/expected.js"></script>
<script src="OfflineAssembler/benchmark.js"></script>
<script>
"use strict";
var results = [];
var benchmark = new OfflineAssemblerBenchmark();
var numIterations = 20;
benchmark.runIterations(numIterations, results);
reportResult(results);
</script>`;


let runOfflineAssemblerBenchmark = null;
if (!isInBrowser) {
    let sources = [
        "benchmark.js"
        , "OfflineAssembler/registers.js"
        , "OfflineAssembler/instructions.js"
        , "OfflineAssembler/ast.js"
        , "OfflineAssembler/parser.js"
        , "OfflineAssembler/file.js"
        , "OfflineAssembler/LowLevelInterpreter.js"
        , "OfflineAssembler/LowLevelInterpreter32_64.js"
        , "OfflineAssembler/LowLevelInterpreter64.js"
        , "OfflineAssembler/InitBytecodes.js"
        , "OfflineAssembler/expected.js"
        , "OfflineAssembler/benchmark.js"
    ];

    runOfflineAssemblerBenchmark = makeBenchmarkRunner(sources, "OfflineAssemblerBenchmark", 20);
}

const OfflineAssemblerBenchmarkRunner = {
    name: "Offline Assembler",
    code: OfflineAssemblerBenchmarkCode,
    run: runOfflineAssemblerBenchmark,
    cells: {}
};

if (isInBrowser) {
    OfflineAssemblerBenchmarkRunner.cells = {
        firstIteration: document.getElementById("OfflineAssemblerFirstIteration"),
        averageWorstCase: document.getElementById("OfflineAssemblerAverageWorstCase"),
        steadyState: document.getElementById("OfflineAssemblerSteadyState"),
        message: document.getElementById("OfflineAssemblerMessage")
    };
}
