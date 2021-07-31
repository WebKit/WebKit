// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindatetime
description: Behavior when provided calendar value is a function
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];
const calendar = function() {};
const timeZone = new Proxy({
  getOffsetNanosecondsFor(instant) {
    actual.push("call timeZone.getOffsetNanosecondsFor");
    return -Number(instant.epochNanoseconds % 86400_000_000_000n);
  },
}, {
  has(target, property) {
    actual.push(`has timeZone.${property}`);
    return property in target;
  },
  get(target, property) {
    actual.push(`get timeZone.${property}`);
    return target[property];
  },
});

Object.defineProperty(Temporal.Calendar, "from", {
  get() {
    actual.push("get Temporal.Calendar.from");
    return undefined;
  },
});

const result = Temporal.Now.plainDateTime(calendar, timeZone);
for (const property of ["hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]) {
  assert.sameValue(result[property], 0, property);
}

assert.compareArray(actual, expected);
