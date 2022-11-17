// Copyright (C) 2022 Igalia S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.mergefields
description: Properties on objects passed to mergeFields() are accessed in the correct order
features: [Temporal]
includes: [compareArray.js, temporalHelpers.js]
---*/

const expected = [
  "ownKeys fields",
  "getOwnPropertyDescriptor fields.year",
  "get fields.year",
  "getOwnPropertyDescriptor fields.month",
  "get fields.month",
  "getOwnPropertyDescriptor fields.day",
  "get fields.day",
  "getOwnPropertyDescriptor fields.extra",
  "get fields.extra",
  "ownKeys additionalFields",
  "getOwnPropertyDescriptor additionalFields.3",
  "get additionalFields.3",
  "getOwnPropertyDescriptor additionalFields.monthCode",
  "get additionalFields.monthCode",
  "getOwnPropertyDescriptor additionalFields[Symbol('extra')]",
  "get additionalFields[Symbol('extra')]",
];
const actual = [];

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 2022,
  month: 10,
  day: 17,
  extra: "extra property",
}, "fields");
const additionalFields = TemporalHelpers.propertyBagObserver(actual, {
  [Symbol("extra")]: "extra symbol property",
  monthCode: "M10",
  3: "extra array index property",
}, "additionalFields");

const instance = new Temporal.Calendar("iso8601");
instance.mergeFields(fields, additionalFields);
assert.compareArray(actual, expected, "order of observable operations");
