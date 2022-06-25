// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: A number is converted to a string, then to Temporal.Calendar
features: [Temporal]
---*/

const instance = new Temporal.PlainDate(1976, 11, 18, { id: "replace-me" });

const arg = 19761118;

const result = instance.withCalendar(arg);
assert.sameValue(result.calendar.id, "iso8601", "19761118 is a valid ISO string for Calendar");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.withCalendar(arg),
    `Number ${arg} does not convert to a valid ISO string for Calendar`
  );
}
