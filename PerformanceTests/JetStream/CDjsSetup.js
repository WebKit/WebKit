// Copyright (C) 2015 Apple Inc. All rights reserved.
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

var CDjsFiles = [
    "constants.js",
    "util.js",
    "red_black_tree.js",
    "call_sign.js",
    "vector_2d.js",
    "vector_3d.js",
    "motion.js",
    "reduce_collision_set.js",
    "simulator.js",
    "collision.js",
    "collision_detector.js",
    "benchmark.js"
];

var code = "";
for (var i = 0; i < CDjsFiles.length; ++i)
    code += "<script src=\"cdjs/" + CDjsFiles[i] + "\"></script>\n";
code += "<script>\n";
code += "console.log(\"running...\");\n";
code += "var __result = benchmark();\n";
code += "console.log(\"got result: \" + __result);\n";
code += "top.JetStream.reportResult(__result);\n";
code += "</script>";

console.log("code = " + code);

JetStream.addPlan({
    name: "cdjs",
    benchmarks: [{
        name: "cdjs",
        category: "Latency",
        unit: "ms"
    }],
    code: code
});

