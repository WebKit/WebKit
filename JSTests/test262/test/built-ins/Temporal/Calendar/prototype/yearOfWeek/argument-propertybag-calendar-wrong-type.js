// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: >
  Appropriate error thrown when a calendar property from a property bag cannot
  be converted to a calendar object or string
features: [BigInt, Symbol, Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.Calendar("iso8601");

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
];

for (const [calendar, description] of rangeErrorTests) {
  let arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(RangeError, () => instance.yearOfWeek(arg), `${description} does not convert to a valid ISO string`);

  arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(RangeError, () => instance.yearOfWeek(arg), `${description} does not convert to a valid ISO string (nested property)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],  // TypeError due to missing dateFromFields()
  [Temporal.Calendar, "Temporal.Calendar, object"],  // ditto
  [Temporal.Calendar.prototype, "Temporal.Calendar.prototype, object"],  // fails brand check in dateFromFields()
];

for (const [calendar, description] of typeErrorTests) {
  let arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(TypeError, () => instance.yearOfWeek(arg), `${description} is not a valid property bag and does not convert to a string`);

  arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(TypeError, () => instance.yearOfWeek(arg), `${description} is not a valid property bag and does not convert to a string (nested property)`);
}

const arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar: undefined } };
assert.throws(RangeError, () => instance.yearOfWeek(arg), `nested undefined calendar property is always a RangeError`);
