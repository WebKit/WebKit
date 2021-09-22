// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.zoneddatetimeiso
description: Functions when time zone argument is omitted
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [];

Object.defineProperty(Temporal.TimeZone, "from", {
  get() {
    actual.push("get Temporal.TimeZone.from");
    return undefined;
  },
});

const systemTimeZone = Temporal.Now.timeZone();

const resultExplicit = Temporal.Now.zonedDateTimeISO(undefined);
assert.sameValue(resultExplicit.timeZone.id, systemTimeZone.id);

assert.compareArray(actual, expected, "Temporal.TimeZone.from should not be called");

const resultImplicit = Temporal.Now.zonedDateTimeISO();
assert.sameValue(resultImplicit.timeZone.id, systemTimeZone.id);

assert.compareArray(actual, expected, "Temporal.TimeZone.from should not be called");
