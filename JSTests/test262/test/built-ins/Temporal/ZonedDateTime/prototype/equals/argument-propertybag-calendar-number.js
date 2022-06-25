// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: A number as calendar in a property bag is converted to a string, then to a calendar
features: [Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.ZonedDateTime(0n, timeZone);

const calendar = 19970327;

let arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result1 = instance.equals(arg);
assert.sameValue(result1, true, "19970327 is a valid ISO string for calendar");

arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar: { calendar } };
const result2 = instance.equals(arg);
assert.sameValue(result2, true, "19970327 is a valid ISO string for calendar (nested property)");

const numbers = [
  1,
  -19970327,
  1234567890,
];

for (const calendar of numbers) {
  let arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
  assert.throws(
    RangeError,
    () => instance.equals(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar`
  );
  arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar: { calendar } };
  assert.throws(
    RangeError,
    () => instance.equals(arg),
    `Number ${calendar} does not convert to a valid ISO string for calendar (nested property)`
  );
}
