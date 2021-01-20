// Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

var Octane2Suites = [
    {name: "richards", category: "Throughput", files: ["richards.js"]},
    {name: "delta-blue", category: "Throughput", files: ["deltablue.js"]},
    {name: "crypto", category: "Throughput", files: ["crypto.js"]},
    {name: "proto-raytracer", category: "Throughput", files: ["raytrace.js"]},
    {name: "earley-boyer", category: "Throughput", files: ["earley-boyer.js"]},
    {name: "regexp-2010", category: "Throughput", files: ["regexp.js"]},
    {name: "splay", category: "Throughput", latency: true, files:["splay.js"]},
    {name: "navier-stokes", category: "Throughput", files: ["navier-stokes.js"]},
    {name: "pdfjs", category: "Throughput", files: ["pdfjs.js"]},
    {name: "mandreel", category: "Throughput", latency: true, files: ["mandreel.js"]},
    {name: "gbemu", category: "Throughput", files: ["gbemu-part1.js", "gbemu-part2.js"]},
    {name: "code-first-load", category: "Latency", files: ["code-load.js"]},
    {name: "box2d", category: "Throughput", files: ["box2d.js"]},
    {name: "zlib", category: "Throughput", files: ["zlib.js", "zlib-data.js"]},
    {name: "typescript", category: "Latency", oneRun: true, files: ["typescript.js", "typescript-input.js", "typescript-compiler.js"]}
];

for (var i = 0; i < Octane2Suites.length; ++i) {
    var suite = Octane2Suites[i];
    var myBenchmarks = [{
        name: suite.name,
        unit: suite.oneRun ? "ms" : "ms/run",
        category: suite.category,
    }];
    if (suite.latency) {
        myBenchmarks.push({
            name: suite.name + "-latency",
            unit: "ms",
            category: "Latency"
        });
    }
    
    var code = "";
    code += "<script src=\"Octane2/base.js\"></script>\n";
    for (var j = 0; j < suite.files.length; ++j)
        code += "<script src=\"Octane2/" + suite.files[j] + "\"></script>\n";
    code += "<script>\n";
    code += "BenchmarkSuite.scores = [];\n";
    code += "var __suite = BenchmarkSuite.suites[0];\n";
    code += "for (var __thing = __suite.RunStep({}); __thing; __thing = __thing());\n";
    code += "top.JetStream.reportResult(\n";
    code += "    BenchmarkSuite.GeometricMeanTime(__suite.results) / 1000,\n";
    code += "    BenchmarkSuite.GeometricMeanLatency(__suite.results) / 1000);\n";
    code += "</script>";
    
    JetStream.addPlan({
        name: suite.name,
        benchmarks: myBenchmarks,
        code: code
    });
}

