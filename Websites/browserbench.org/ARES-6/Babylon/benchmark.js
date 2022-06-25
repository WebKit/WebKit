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

class BabylonBenchmark {
    constructor(verbose = 0)
    {
        let sources = [];

        const files = [
            ["./Babylon/air-blob.js", {}]
            , ["./Babylon/basic-blob.js", {}]
            , ["./Babylon/inspector-blob.js", {}]
            , ["./Babylon/babylon-blob.js", {sourceType: "module"}]
        ];

        for (let [file, options] of files) {
            function appendSource(s) {
                sources.push([file, s, options]);
            }

            let s;
            const isInBrowser = typeof window !== "undefined";
            if (isInBrowser) {
                let request = new XMLHttpRequest();
                request.open('GET', file, false);
                request.send(null);
                if (!request.responseText.length)
                    throw new Error("Expect non-empty sources");
                appendSource(request.responseText);
            } else {
                appendSource(read(file));
            }
        }

        this.sources = sources;
    }

    runIteration()
    {
        const Parser = parserIndexJS;
        const { plugins } = parserIndexJS;
        const { types : tokTypes } = tokenizerTypesJS;
        const estreePlugin = pluginsEstreeJS;
        const flowPlugin = pluginsFlowJS;
        const jsxPlugin = pluginsJsxIndexJS;
        plugins.estree = estreePlugin;
        plugins.flow = flowPlugin;
        plugins.jsx = jsxPlugin;

        function parse(input, options) {
            return new Parser(options, input).parse();
        }

        function parseExpression(input, options) {
            const parser = new Parser(options, input);
            if (parser.options.strictMode) {
                parser.state.strict = true;
            }
            return parser.getExpression();
        }

        for (let [fileName, source, options] of this.sources) {
            parse(source, options);
        }
    }
}

function runBenchmark()
{
    const verbose = 0;
    const numIterations = 150;

    let before = currentTime();

    let benchmark = new Benchmark(verbose);

    for (let iteration = 0; iteration < numIterations; ++iteration)
        benchmark.runIteration();

    let after = currentTime();
    return after - before;
}
