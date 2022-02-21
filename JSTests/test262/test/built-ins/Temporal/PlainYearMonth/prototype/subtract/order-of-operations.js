// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Properties on an object passed to subtract() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainYearMonth(2000, 5);
const expected = [
  "get days",
  "get days.valueOf",
  "call days.valueOf",
  "get hours",
  "get hours.valueOf",
  "call hours.valueOf",
  "get microseconds",
  "get microseconds.valueOf",
  "call microseconds.valueOf",
  "get milliseconds",
  "get milliseconds.valueOf",
  "call milliseconds.valueOf",
  "get minutes",
  "get minutes.valueOf",
  "call minutes.valueOf",
  "get months",
  "get months.valueOf",
  "call months.valueOf",
  "get nanoseconds",
  "get nanoseconds.valueOf",
  "call nanoseconds.valueOf",
  "get seconds",
  "get seconds.valueOf",
  "call seconds.valueOf",
  "get weeks",
  "get weeks.valueOf",
  "call weeks.valueOf",
  "get years",
  "get years.valueOf",
  "call years.valueOf",
];
const actual = [];
const fields = {
  years: 1,
  months: 1,
  weeks: 1,
  days: 1,
  hours: 1,
  minutes: 1,
  seconds: 1,
  milliseconds: 1,
  microseconds: 1,
  nanoseconds: 1,
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
const result = instance.subtract(argument);
TemporalHelpers.assertPlainYearMonth(result, 1999, 4, "M04");
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
assert.compareArray(actual, expected, "order of operations");
