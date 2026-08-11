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

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " " + expected);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

let validRoundingMode = [
    "ceil",
    "floor",
    "expand",
    "halfCeil",
    "halfExpand",
    "halfFloor",
    "halfTrunc",
    "halfEven",
    "trunc",
];

let invalidRoundingMode = [
    "ceiling",
    "down",
    "Down",
    "flooring",
    "halfDown",
    "halfUp",
    "halfup",
    "halfeven",
    "halfdown",
    "half-up",
    "half-even",
    "half-down",
    "up",
    "Up",
];

validRoundingMode.forEach(function(roundingMode) {
    let nf = new Intl.NumberFormat(undefined, {roundingMode});
    shouldBe(roundingMode, nf.resolvedOptions().roundingMode);
});

invalidRoundingMode.forEach(function(roundingMode) {
    shouldThrow(() => {
        let nf = new Intl.NumberFormat(undefined, {roundingMode}); }, `RangeError: roundingMode must be either "ceil", "floor", "expand", "trunc", "halfCeil", "halfFloor", "halfExpand", "halfTrunc", or "halfEven"`);
});

// Check default is "halfExpand"
shouldBe("halfExpand", (new Intl.NumberFormat().resolvedOptions().roundingMode));
shouldBe("halfExpand", (new Intl.NumberFormat(
    undefined, {roundingMode: undefined}).resolvedOptions().roundingMode));

// Check roundingMode is read once after reading signDisplay

let read = [];
let options = {
    get signDisplay() { read.push('signDisplay'); return undefined; },
    get roundingMode() { read.push('roundingMode'); return undefined; },
};

new Intl.NumberFormat(undefined, options);
shouldBe("roundingMode,signDisplay", read.join(","));
