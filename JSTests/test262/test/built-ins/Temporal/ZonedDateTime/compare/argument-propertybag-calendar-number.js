// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const calendar = 19970327;

const timeZone = new Temporal.TimeZone("UTC");
const datetime = new Temporal.ZonedDateTime(0n, timeZone);

const arg = { year: 1970, monthCode: "M01", day: 1, calendar, timeZone };
const result1 = Temporal.ZonedDateTime.compare(arg, datetime);
assert.sameValue(result1, 0, "19970327 is a valid ISO string for calendar (first argument)");
const result2 = Temporal.ZonedDateTime.compare(datetime, arg);
assert.sameValue(result2, 0, "19970327 is a valid ISO string for calendar (second argument)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1970, monthCode: "M01", day: 1, calendar, timeZone };
  assert.throws(
    RangeError,
    () => Temporal.ZonedDateTime.compare(arg, datetime),
    `Number ${calendar} does not convert to a valid ISO string for calendar (first argument)`
  );
  assert.throws(
    RangeError,
    () => Temporal.ZonedDateTime.compare(datetime, arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (second argument)`
  );
}
