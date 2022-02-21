// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.add
description: Properties on an object passed to add() are accessed in the correct order
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const instance = new Temporal.Instant(10n);
const expected = [
  "get days",
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
  "get nanoseconds",
  "get nanoseconds.valueOf",
  "call nanoseconds.valueOf",
  "get seconds",
  "get seconds.valueOf",
  "call seconds.valueOf",
  "get weeks",
  "get years",
];
const actual = [];
const fields = {
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
const result = instance.add(argument);
assert.sameValue(result.epochNanoseconds, 3661001001011n, "epochNanoseconds result");
assert.compareArray(actual, expected, "order of operations");
