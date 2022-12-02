// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  Appropriate error thrown when relativeTo.calendar cannot be converted to a
  calendar object or string
features: [BigInt, Symbol, Temporal]
---*/

const timeZone = new Temporal.TimeZone("UTC");
const instance = new Temporal.Duration(1, 0, 0, 0, 24);

const rangeErrorTests = [
  [null, "null"],
  [true, "boolean"],
  ["", "empty string"],
  [1, "number that doesn't convert to a valid ISO string"],
  [1n, "bigint"],
  [new Temporal.TimeZone("UTC"), "time zone instance"],
];

for (const [calendar, description] of rangeErrorTests) {
  let relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), `${description} does not convert to a valid ISO string`);

  relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), `${description} does not convert to a valid ISO string (nested property)`);
}

const typeErrorTests = [
  [Symbol(), "symbol"],
  [{}, "plain object"],
  [Temporal.PlainDate, "Temporal.PlainDate, object"],
  [Temporal.PlainDate.prototype, "Temporal.PlainDate.prototype, object"],
  [Temporal.ZonedDateTime, "Temporal.ZonedDateTime, object"],
  [Temporal.ZonedDateTime.prototype, "Temporal.ZonedDateTime.prototype, object"],
];

for (const [calendar, description] of typeErrorTests) {
  let relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar };
  assert.throws(TypeError, () => instance.total({ unit: "days", relativeTo }), `${description} is not a valid property bag and does not convert to a string`);

  relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar } };
  assert.throws(TypeError, () => instance.total({ unit: "days", relativeTo }), `${description} is not a valid property bag and does not convert to a string (nested property)`);
}

const relativeTo = { year: 2019, monthCode: "M11", day: 1, calendar: { calendar: undefined } };
assert.throws(RangeError, () => instance.total({ unit: "days", relativeTo }), `nested undefined calendar property is always a RangeError`);
