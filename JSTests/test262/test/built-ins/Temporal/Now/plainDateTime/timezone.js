// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: The value returned by TimeZone.getOffsetNanosecondsFor affects the result
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor(instant) {
    assert.sameValue(instant instanceof Temporal.Instant, true, "Instant");
    return -Number(instant.epochNanoseconds % 86400_000_000_000n);
  },
});

const calendar = Temporal.Calendar.from("iso8601");

const result = Temporal.Now.plainDateTime(calendar, timeZone);
for (const property of ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]) {
  assert.sameValue(result[property], 0, property);
}

assert.compareArray(actual, expected);
