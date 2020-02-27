// Copyright 2020 Google Inc, Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-initializedatetimeformat
description: >
    Tests that invalid numberingSystem and calendar option throws RangeError.
author: Caio Lima
---*/

let invalidValues = ["ab", "aaaaaaaabbbbbababa."];

var defaultLocale = new Intl.NumberFormat().resolvedOptions().locale;

invalidValues.forEach(function (value) {
    assert.throws(RangeError, function () {
            return new Intl.DateTimeFormat([defaultLocale], {numberingSystem: value});
    }, "Invalid numberingSystem value " + value + " was not rejected.");
    assert.throws(RangeError, function () {
            return new Intl.DateTimeFormat([defaultLocale + "-u-nu-" + value]);
    }, "Invalid numberingSystem value " + value + " was not rejected.");

    assert.throws(RangeError, function () {
            return new Intl.DateTimeFormat([defaultLocale], {calendar: value});
    }, "Invalid calendar value " + value + " was not rejected.");
    assert.throws(RangeError, function () {
            return new Intl.DateTimeFormat([defaultLocale + "-u-ca-" + value]);
    }, "Invalid calendar value " + value + " was not rejected.");
});

