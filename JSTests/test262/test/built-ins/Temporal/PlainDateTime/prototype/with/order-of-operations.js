// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
description: Properties on an object passed to with() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
const expected = [
  "get calendar",
  "get timeZone",
  "get day",
  "get day.valueOf",
  "call day.valueOf",
  "get hour",
  "get hour.valueOf",
  "call hour.valueOf",
  "get microsecond",
  "get microsecond.valueOf",
  "call microsecond.valueOf",
  "get millisecond",
  "get millisecond.valueOf",
  "call millisecond.valueOf",
  "get minute",
  "get minute.valueOf",
  "call minute.valueOf",
  "get month",
  "get month.valueOf",
  "call month.valueOf",
  "get monthCode",
  "get monthCode.toString",
  "call monthCode.toString",
  "get nanosecond",
  "get nanosecond.valueOf",
  "call nanosecond.valueOf",
  "get second",
  "get second.valueOf",
  "call second.valueOf",
  "get year",
  "get year.valueOf",
  "call year.valueOf",
];
const actual = [];
const fields = {
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
};
const argument = new Proxy(fields, {
  get(target, key) {
    actual.push(`get ${key}`);
    const result = target[key];
    if (result === undefined) {
      return undefined;
    }
    return TemporalHelpers.toPrimitiveObserver(actual, result, key);
  },
  has(target, key) {
    actual.push(`has ${key}`);
    return key in target;
  },
});
const result = instance.with(argument);
TemporalHelpers.assertPlainDateTime(result, 1, 1, "M01", 1, 1, 1, 1, 1, 1, 1);
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
assert.compareArray(actual, expected, "order of operations");
