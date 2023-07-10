// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.tojson
description: toJSON() returns the internal slot value
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];

const calendar = new Temporal.Calendar("iso8601");
TemporalHelpers.observeProperty(actual, calendar, Symbol.toPrimitive, undefined);
TemporalHelpers.observeProperty(actual, calendar, "id", "bogus");
TemporalHelpers.observeProperty(actual, calendar, "toString", function () {
  actual.push("call calendar.toString");
  return "gregory";
});

const result = calendar.toJSON();
assert.sameValue(result, "iso8601", "toJSON gets the internal slot value");
assert.compareArray(actual, [], "should not invoke any observable operations");
