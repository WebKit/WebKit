// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.calendar.prototype.id
description: Getter does not call toString(), returns the ID from internal slot
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [];

const calendar = new Temporal.Calendar("iso8601");
TemporalHelpers.observeProperty(actual, calendar, Symbol.toPrimitive, undefined);
TemporalHelpers.observeProperty(actual, calendar, "toString", function () {
  actual.push("call calendar.toString");
  return "calendar ID";
});

const result = calendar.id;
assert.compareArray(actual, expected);
assert.sameValue(result, "iso8601");
