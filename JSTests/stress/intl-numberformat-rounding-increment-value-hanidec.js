// Copyright 2021 Apple Inc. All rights reserved.
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

let penny = new Intl.NumberFormat("ja-JP", { numberingSystem: 'hanidec', minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 1 });
let nickel = new Intl.NumberFormat("ja-JP", { numberingSystem: 'hanidec', minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 5 });
let dime = new Intl.NumberFormat("ja-JP", { numberingSystem: 'hanidec', minimumFractionDigits: 2, maximumFractionDigits: 2, roundingIncrement: 10 });

shouldBe("一〇.一五", penny.format(10.154));
shouldBe("一〇.一六", penny.format(10.155));
shouldBe("一〇.一〇", nickel.format(10.124));
shouldBe("一〇.一五", nickel.format(10.125));
shouldBe("一〇.四〇", dime.format(10.444));
shouldBe("一〇.四〇", dime.format(10.445));
shouldBe("一〇.四〇", dime.format(10.445));
shouldBe("一〇.五〇", dime.format(10.45));
