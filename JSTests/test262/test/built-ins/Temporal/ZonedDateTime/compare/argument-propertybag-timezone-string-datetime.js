// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: Conversion of ISO date-time strings to Temporal.TimeZone instances
features: [Temporal]
---*/

const instance = new Temporal.ZonedDateTime(0n, "UTC");

let timeZone = "2021-08-19T17:30";
assert.throws(RangeError, () => Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, timeZone }, instance), "bare date-time string is not a time zone (arg 1)");
assert.throws(RangeError, () => Temporal.ZonedDateTime.compare(instance, { year: 2000, month: 5, day: 2, timeZone }), "bare date-time string is not a time zone (arg 2)");
assert.throws(RangeError, () => Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, timeZone: { timeZone } }, instance), "bare date-time string is not a time zone (arg 1)");
assert.throws(RangeError, () => Temporal.ZonedDateTime.compare(instance, { year: 2000, month: 5, day: 2, timeZone: { timeZone } }), "bare date-time string is not a time zone (arg 2)");

// The following are all valid strings so should not throw:

[
  "2021-08-19T17:30Z",
  "2021-08-19T17:30-07:00",
  "2021-08-19T17:30[UTC]",
  "2021-08-19T17:30Z[UTC]",
  "2021-08-19T17:30-07:00[UTC]",
].forEach((timeZone) => {
  Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, timeZone }, instance);
  Temporal.ZonedDateTime.compare(instance, { year: 2000, month: 5, day: 2, timeZone });
  Temporal.ZonedDateTime.compare({ year: 2000, month: 5, day: 2, timeZone: { timeZone } }, instance);
  Temporal.ZonedDateTime.compare(instance, { year: 2000, month: 5, day: 2, timeZone: { timeZone } });
});
