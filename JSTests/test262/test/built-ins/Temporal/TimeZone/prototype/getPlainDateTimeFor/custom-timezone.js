// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: getOffsetNanosecondsFor is called by getPlainDateTimeFor
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456789Z");
const timeZone = new Temporal.TimeZone("UTC");
TemporalHelpers.observeProperty(actual, timeZone, "getOffsetNanosecondsFor", function (instantArg) {
  actual.push("call timeZone.getOffsetNanosecondsFor");
  assert.sameValue(instantArg, instant);
  return 9876543210123;
});

const result = timeZone.getPlainDateTimeFor(instant);
TemporalHelpers.assertPlainDateTime(result, 1975, 2, "M02", 2, 17, 10, 12, 666, 666, 912);
assert.compareArray(actual, expected);
