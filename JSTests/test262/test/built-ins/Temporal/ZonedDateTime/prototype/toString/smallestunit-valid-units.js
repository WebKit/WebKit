// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.tostring
description: Valid units for the smallestUnit option
features: [Temporal]
---*/

const datetime = new Temporal.ZonedDateTime(1_000_000_000_123_456_789n, "UTC");

assert.sameValue(datetime.toString({ smallestUnit: "minute" }), "2001-09-09T01:46+00:00[UTC]");
assert.sameValue(datetime.toString({ smallestUnit: "second" }), "2001-09-09T01:46:40+00:00[UTC]");
assert.sameValue(datetime.toString({ smallestUnit: "millisecond" }), "2001-09-09T01:46:40.123+00:00[UTC]");
assert.sameValue(datetime.toString({ smallestUnit: "microsecond" }), "2001-09-09T01:46:40.123456+00:00[UTC]");
assert.sameValue(datetime.toString({ smallestUnit: "nanosecond" }), "2001-09-09T01:46:40.123456789+00:00[UTC]");

const notValid = [
  "year",
  "month",
  "week",
  "day",
  "hour",
];

notValid.forEach((smallestUnit) => {
  assert.throws(RangeError, () => datetime.toString({ smallestUnit }), smallestUnit);
});
