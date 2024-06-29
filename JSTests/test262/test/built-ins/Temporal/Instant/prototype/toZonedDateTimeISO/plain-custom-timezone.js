// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetimeiso
description: TimeZone.getPlainDateTimeFor is not called
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.getOffsetNanosecondsFor",
  "has timeZone.getPossibleInstantsFor",
  "has timeZone.id",
];

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456789Z");
const calendar = Temporal.Calendar.from("iso8601");
const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getPlainDateTimeFor: Temporal.PlainDateTime.from("1963-07-02T12:00:00.987654321"),
});

const result = instant.toZonedDateTimeISO(timeZone);
assert.sameValue(result.epochNanoseconds, instant.epochNanoseconds);

assert.compareArray(actual, expected);
