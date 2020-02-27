// Copyright 2020 Google Inc, Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializenumberformat
description: >
    Tests that invalid numberingSystem option throws RangeError.
author: Caio Lima
---*/

let invalidValues = ["ab", "aaaaaaaabbbbbababa."];

var defaultLocale = new Intl.NumberFormat().resolvedOptions().locale;

invalidValues.forEach(function (value) {
    assert.throws(RangeError, function () {
            return new Intl.NumberFormat([defaultLocale], {numberingSystem: value});
    }, "Invalid numberingSystem value " + value + " was not rejected.");
    assert.throws(RangeError, function () {
            return new Intl.NumberFormat([defaultLocale + "-u-nu-" + value]);
    }, "Invalid numberingSystem value " + value + " was not rejected.");
});
