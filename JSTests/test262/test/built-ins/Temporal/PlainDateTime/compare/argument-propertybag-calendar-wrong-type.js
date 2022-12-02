// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: >
  Appropriate error thrown when a calendar property from a property bag cannot
  be converted to a calendar object or string
features: [BigInt, Symbol, Temporal]
---*/

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
  [new Temporal.TimeZone("UTC"), "time zone instance"],
];

for (const [calendar, description] of rangeErrorTests) {
  let arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(RangeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `${description} does not convert to a valid ISO string (first argument)`);
  assert.throws(RangeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `${description} does not convert to a valid ISO string (second argument)`);

  arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(RangeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `${description} does not convert to a valid ISO string (nested property, first argument)`);
  assert.throws(RangeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `${description} does not convert to a valid ISO string (nested property, second argument)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],  // TypeError due to missing dateFromFields()
  [Temporal.Calendar, "Temporal.Calendar, object"],  // ditto
  [Temporal.Calendar.prototype, "Temporal.Calendar.prototype, object"],  // fails brand check in dateFromFields()
];

for (const [calendar, description] of typeErrorTests) {
  let arg = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `${description} is not a valid property bag and does not convert to a string (first argument)`);
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `${description} is not a valid property bag and does not convert to a string (second argument)`);

  arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `${description} is not a valid property bag and does not convert to a string (nested property, first argument)`);
  assert.throws(TypeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `${description} is not a valid property bag and does not convert to a string (nested property, second argument)`);
}

const arg = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar: undefined } };
assert.throws(RangeError, () => Temporal.PlainDateTime.compare(arg, new Temporal.PlainDateTime(1976, 11, 18)), `nested undefined calendar property is always a RangeError (first argument)`);
assert.throws(RangeError, () => Temporal.PlainDateTime.compare(new Temporal.PlainDateTime(1976, 11, 18), arg), `nested undefined calendar property is always a RangeError (second argument)`);
