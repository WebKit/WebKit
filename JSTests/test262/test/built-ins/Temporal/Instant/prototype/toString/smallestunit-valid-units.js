// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: Valid units for the smallestUnit option
features: [Temporal]
---*/

const instant = new Temporal.Instant(1_000_000_000_123_456_789n);

assert.sameValue(instant.toString({ smallestUnit: "minute" }), "2001-09-09T01:46Z");
assert.sameValue(instant.toString({ smallestUnit: "second" }), "2001-09-09T01:46:40Z");
assert.sameValue(instant.toString({ smallestUnit: "millisecond" }), "2001-09-09T01:46:40.123Z");
assert.sameValue(instant.toString({ smallestUnit: "microsecond" }), "2001-09-09T01:46:40.123456Z");
assert.sameValue(instant.toString({ smallestUnit: "nanosecond" }), "2001-09-09T01:46:40.123456789Z");

const notValid = [
  "era",
  "year",
  "month",
  "week",
  "day",
  "hour",
];

notValid.forEach((smallestUnit) => {
  assert.throws(RangeError, () => instant.toString({ smallestUnit }), smallestUnit);
});
