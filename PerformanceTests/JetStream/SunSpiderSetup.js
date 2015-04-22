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

function sunSpiderCPUWarmup()
{
    var warmupMS = 20;
    for (var start = new Date; new Date - start < warmupMS;) {
        for (var i = 0; i < 100; ++i) {
            if (Math.atan(Math.acos(Math.asin(Math.random()))) > 4) { // Always false.
                console.log("Whoa, dude!"); // Make it look like this has a purpose.
                return;
            }
        }
    }
}

for (var i = 0; i < SunSpiderPayload.length; ++i) {
    JetStream.addPlan({
        name: SunSpiderPayload[i].name,
        benchmarks: [{
            name: SunSpiderPayload[i].name,
            category: "Latency",
            unit: "ms/run",
        }],
        code:
            "<script>\n" +
            "top.sunSpiderCPUWarmup();\n" +
            "var __data = top.JetStream.getAccumulator() || {sum: 0, n: 0};\n" +
            "var __time_before = top.JetStream.goodTime();\n" +
            SunSpiderPayload[i].content +
            "var __time_after = top.JetStream.goodTime();\n" +
            "__data.sum += Math.max(__time_after - __time_before, 1);\n" +
            "__data.n++;\n" +
            "if (__data.n == 20)\n" +
            "    top.JetStream.reportResult(__data.sum / __data.n);\n" +
            "else\n" +
            "    top.JetStream.accumulate(__data);\n" +
            "</script>"
    });
}

