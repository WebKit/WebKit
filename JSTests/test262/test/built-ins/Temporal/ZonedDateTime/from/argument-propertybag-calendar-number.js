// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: A number as calendar in a property bag is not accepted
features: [Temporal]
---*/
const timeZone = "UTC";

const numbers = [
  1,
  19970327,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
  assert.throws(
    TypeError,
    () => Temporal.ZonedDateTime.from(arg),
    "Numbers cannot be used as a calendar"
  );
}
