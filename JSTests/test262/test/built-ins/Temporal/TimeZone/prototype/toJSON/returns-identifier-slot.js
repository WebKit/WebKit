// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.tojson
description: toJSON() returns the internal slot value
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];

const timeZone = new Temporal.TimeZone("UTC");
TemporalHelpers.observeProperty(actual, timeZone, Symbol.toPrimitive, undefined);
TemporalHelpers.observeProperty(actual, timeZone, "id", "Etc/Bogus");
TemporalHelpers.observeProperty(actual, timeZone, "toString", function () {
  actual.push("call timeZone.toString");
  return "Etc/TAI";
});

const result = timeZone.toJSON();
assert.sameValue(result, "UTC", "toJSON gets the internal slot value");
assert.compareArray(actual, [], "should not invoke any observable operations");
