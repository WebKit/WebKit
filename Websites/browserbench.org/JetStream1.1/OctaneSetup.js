// Copyright (C) 2014 Apple Inc. All rights reserved.
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

var OctaneSuites = [
    {name: "code-multi-load", category: "Latency", files: ["code-load.js"]}
];

for (var i = 0; i < OctaneSuites.length; ++i) {
    var suite = OctaneSuites[i];
    var myBenchmarks = [{
        name: suite.name,
        unit: "ms/run",
        category: suite.category,
    }];
    
    var code = "";
    code += "<script src=\"Octane/base.js\"></script>\n";
    for (var j = 0; j < suite.files.length; ++j)
        code += "<script src=\"Octane/" + suite.files[j] + "\"></script>\n";
    code += "<script>\n";
    code += "BenchmarkSuite.scores = [];\n";
    code += "var __suite = BenchmarkSuite.suites[0];\n";
    code += "for (var __thing = __suite.RunStep({}); __thing; __thing = __thing());\n";
    code += "top.JetStream.reportResult(\n";
    code += "    BenchmarkSuite.GeometricMean(__suite.results) / 1000);\n";
    code += "</script>";
    
    JetStream.addPlan({
        name: suite.name,
        benchmarks: myBenchmarks,
        code: code
    });
}

