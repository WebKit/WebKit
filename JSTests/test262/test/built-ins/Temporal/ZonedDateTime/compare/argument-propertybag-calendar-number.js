// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: A number as calendar in a property bag is not accepted
features: [Temporal]
---*/

const timeZone = "UTC";
const datetime = new Temporal.ZonedDateTime(0n, timeZone);

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1970, monthCode: "M01", day: 1, calendar, timeZone };
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.compare(arg, datetime),
    "A number is not a valid ISO string for calendar (first argument)"
  );
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.compare(datetime, arg),
    "A number is not a valid ISO string for calendar (second argument)"
  );
}
