// Copyright 2021 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected + " " + message);
}

// Check the rounding behavior.
// Based on https://tc39.es/proposal-intl-numberformat-v3/out/numberformat/diff.html#table-intl-rounding-modes
let inputs = [-1.5, 0.4, 0.5, 0.6, 1.5];
let expectations = {
    "ceil":       ["-1", "1", "1", "1", "2"],
    "floor":      ["-2", "0", "0", "0", "1"],
    "expand":     ["-2", "1", "1", "1", "2"],
    "trunc":      ["-1", "0", "0", "0", "1"],
    "halfCeil":   ["-1", "0", "1", "1", "2"],
    "halfFloor":  ["-2", "0", "0", "1", "1"],
    "halfExpand": ["-2", "0", "1", "1", "2"],
    "halfTrunc":  ["-1", "0", "0", "1", "1"],
    "halfEven":   ["-2", "0", "0", "1", "2"],
};
Object.keys(expectations).forEach(function(roundingMode) {
    let exp = expectations[roundingMode];
    let idx = 0;
    let nf = new Intl.NumberFormat("en", {roundingMode, maximumFractionDigits: 0});
    shouldBe(roundingMode, nf.resolvedOptions().roundingMode);
    inputs.forEach(function(input) {
        let msg = "input: " + input + " with roundingMode: " + roundingMode;
        if ($vm.icuVersion() >= 69)
            shouldBe(exp[idx++], nf.format(input), msg);
        else if (roundingMode !== "halfCeil" && roundingMode !== "halfFloor")
            shouldBe(exp[idx++], nf.format(input), msg);
        else
            nf.format(input) // should not throw.
    })
});
