// Copyright 2020 the V8 project authors. All rights reserved.
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
        throw new Error('bad value: ' + actual);
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

let validUseGrouping = [
    "min2",
    "auto",
    "always",
    false,
];

let nonListedUseGrouping = [
    "min-2",
];

let specialUseGrouping = [
    "true",
    "false",
]

validUseGrouping.forEach(function(useGrouping) {
    let nf = new Intl.NumberFormat(undefined, {useGrouping});
    shouldBe(useGrouping, nf.resolvedOptions().useGrouping);
});

nonListedUseGrouping.forEach(function(useGrouping) {
    shouldThrow(() => {
        let nf = new Intl.NumberFormat(undefined, {useGrouping});
        nf.resolvedOptions().useGrouping
    }, `RangeError: useGrouping must be either true, false, "min2", "auto", or "always"`);
});

specialUseGrouping.forEach(function(useGrouping) {
    let nf = new Intl.NumberFormat(undefined, {useGrouping});
    shouldBe("auto", nf.resolvedOptions().useGrouping);
});

// useGrouping: undefined get "auto"
shouldBe("auto",
    (new Intl.NumberFormat()).resolvedOptions().useGrouping);
shouldBe("auto",
    (new Intl.NumberFormat(undefined, {useGrouping: undefined}))
    .resolvedOptions().useGrouping);

// useGrouping: true get "always"
shouldBe("always",
    (new Intl.NumberFormat(undefined, {useGrouping: true}))
    .resolvedOptions().useGrouping);

// useGrouping: false get false
// useGrouping: "" get false
shouldBe(false,
    (new Intl.NumberFormat(undefined, {useGrouping: false}))
    .resolvedOptions().useGrouping);
shouldBe(false,
    (new Intl.NumberFormat(undefined, {useGrouping: ""}))
    .resolvedOptions().useGrouping);

// Some locales with default minimumGroupingDigits
let mgd1 = ["en"];
// Some locales with default minimumGroupingDigits{"2"}
let mgd2 = ["es", "pl", "lv", "et", "bg"];
let all = mgd1.concat(mgd2);

// Check "always"
all.forEach(function(locale) {
    let off = new Intl.NumberFormat(locale, {useGrouping: false});
    let msg = "locale: " + locale + " useGrouping: false";
    // In useGrouping: false, no grouping.
    shouldBe(3, off.format(123).length, msg);
    shouldBe(4, off.format(1234).length, msg);
    shouldBe(5, off.format(12345).length, msg);
    shouldBe(6, off.format(123456).length, msg);
    shouldBe(7, off.format(1234567).length, msg);
});

// Check false
all.forEach(function(locale) {
    let always = new Intl.NumberFormat(locale, {useGrouping: "always"});
    let msg = "locale: " + locale + " useGrouping: 'always'";
    shouldBe(3, always.format(123).length);
    // In useGrouping: "always", has grouping when more than 3 digits..
    shouldBe(4 + 1, always.format(1234).length, msg);
    shouldBe(5 + 1, always.format(12345).length, msg);
    shouldBe(6 + 1, always.format(123456).length, msg);
    shouldBe(7 + 2, always.format(1234567).length, msg);
});

// Check "min2"
all.forEach(function(locale) {
    let always = new Intl.NumberFormat(locale, {useGrouping: "min2"});
    let msg = "locale: " + locale + " useGrouping: 'min2'";
    shouldBe(3, always.format(123).length);
    // In useGrouping: "min2", no grouping for 4 digits but has grouping
    // when more than 4 digits..
    shouldBe(4, always.format(1234).length, msg);
    shouldBe(5 + 1, always.format(12345).length, msg);
    shouldBe(6 + 1, always.format(123456).length, msg);
    shouldBe(7 + 2, always.format(1234567).length, msg);
});

// Check "auto"
mgd1.forEach(function(locale) {
    let auto = new Intl.NumberFormat(locale, {useGrouping: "auto"});
    let msg = "locale: " + locale + " useGrouping: 'auto'";
    shouldBe(3, auto.format(123).length, msg);
    shouldBe(4 + 1, auto.format(1234).length, msg);
    shouldBe(5 + 1, auto.format(12345).length, msg);
    shouldBe(6 + 1, auto.format(123456).length, msg);
    shouldBe(7 + 2, auto.format(1234567).length, msg);
});
mgd2.forEach(function(locale) {
    let auto = new Intl.NumberFormat(locale, {useGrouping: "auto"});
    let msg = "locale: " + locale + " useGrouping: 'auto'";
    shouldBe(3, auto.format(123).length, msg);
    // In useGrouping: "auto", since these locales has
    // minimumGroupingDigits{"2"}, no grouping for 4 digits but has grouping
    // when more than 4 digits..
    shouldBe(4, auto.format(1234).length, msg);
    shouldBe(5 + 1, auto.format(12345).length, msg);
    shouldBe(6 + 1, auto.format(123456).length, msg);
    shouldBe(7 + 2, auto.format(1234567).length, msg);
});
