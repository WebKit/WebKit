// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const calendar = 19970327;

const timeZone = new Temporal.TimeZone("UTC");
const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result = Temporal.ZonedDateTime.from(arg);
assert.sameValue(result.calendarId, "iso8601", "19970327 is a valid ISO string for calendar");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
  assert.throws(
    RangeError,
    () => Temporal.ZonedDateTime.from(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
}
