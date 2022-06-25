// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.timezone.prototype.id
description: Getter calls toString() and returns its value
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get [Symbol.toPrimitive]",
  "get toString",
  "call timeZone.toString",
];

const timeZone = new Temporal.TimeZone("UTC");
TemporalHelpers.observeProperty(actual, timeZone, Symbol.toPrimitive, undefined);
TemporalHelpers.observeProperty(actual, timeZone, "toString", function () {
  actual.push("call timeZone.toString");
  return "time zone";
});

const result = timeZone.id;
assert.compareArray(actual, expected);
assert.sameValue(result, "time zone");
