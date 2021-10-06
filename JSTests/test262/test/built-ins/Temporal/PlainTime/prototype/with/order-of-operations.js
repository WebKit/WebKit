// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.with
description: Properties on an object passed to with() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321);
const expected = [
  "get calendar",
  "get timeZone",
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
  "get nanosecond",
  "get nanosecond.valueOf",
  "call nanosecond.valueOf",
  "get second",
  "get second.valueOf",
  "call second.valueOf",
];
const actual = [];
const fields = {
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
TemporalHelpers.assertPlainTime(result, 1, 1, 1, 1, 1, 1);
assert.sameValue(result.calendar.id, "iso8601", "calendar result");
assert.compareArray(actual, expected, "order of operations");
