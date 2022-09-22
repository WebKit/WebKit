// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Properties on objects passed to yearMonthFromFields() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
  "get options.overflow",
  "get options.overflow.toString",
  "call options.overflow.toString",
];
const actual = [];

const instance = new Temporal.Calendar("iso8601");

const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
}, "fields");

const options = TemporalHelpers.propertyBagObserver(actual, {
  overflow: "reject",
}, "options");

const result = instance.yearMonthFromFields(fields, options);
TemporalHelpers.assertPlainYearMonth(result, 1, 1, "M01", "yearMonth result");
assert.sameValue(result.calendar, instance, "calendar result");
assert.compareArray(actual, expected, "order of operations");
