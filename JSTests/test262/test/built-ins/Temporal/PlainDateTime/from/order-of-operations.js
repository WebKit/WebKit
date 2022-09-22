// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.from
description: Properties on an object passed to from() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get fields.calendar",
  "get fields.day",
  "get fields.day.valueOf",
  "call fields.day.valueOf",
  "get fields.hour",
  "get fields.hour.valueOf",
  "call fields.hour.valueOf",
  "get fields.microsecond",
  "get fields.microsecond.valueOf",
  "call fields.microsecond.valueOf",
  "get fields.millisecond",
  "get fields.millisecond.valueOf",
  "call fields.millisecond.valueOf",
  "get fields.minute",
  "get fields.minute.valueOf",
  "call fields.minute.valueOf",
  "get fields.month",
  "get fields.month.valueOf",
  "call fields.month.valueOf",
  "get fields.monthCode",
  "get fields.monthCode.toString",
  "call fields.monthCode.toString",
  "get fields.nanosecond",
  "get fields.nanosecond.valueOf",
  "call fields.nanosecond.valueOf",
  "get fields.second",
  "get fields.second.valueOf",
  "call fields.second.valueOf",
  "get fields.year",
  "get fields.year.valueOf",
  "call fields.year.valueOf",
];
const actual = [];
const fields = TemporalHelpers.propertyBagObserver(actual, {
  year: 1.7,
  month: 1.7,
  monthCode: "M01",
  day: 1.7,
  hour: 1.7,
  minute: 1.7,
  second: 1.7,
  millisecond: 1.7,
  microsecond: 1.7,
  nanosecond: 1.7,
}, "fields");
const result = Temporal.PlainDateTime.from(fields);
TemporalHelpers.assertPlainDateTime(result, 1, 1, "M01", 1, 1, 1, 1, 1, 1, 1);
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
assert.compareArray(actual, expected, "order of operations");
