// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplaindatetime
description: timeZone.getOffsetNanosecondsFor() called
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

const timeZone = new Proxy({
  getOffsetNanosecondsFor() {
    actual.push("call timeZone.getOffsetNanosecondsFor");
    return -8735135802468;
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

const zdt = new Temporal.ZonedDateTime(160583136123456789n, timeZone);
const dateTime = Temporal.PlainDateTime.from("1975-02-02T12:00:00.987654321");
const result = zdt.toPlainDateTime();
for (const property of ["year", "month", "day", "hour", "minute", "second", "millisecond", "microsecond", "nanosecond"]) {
  assert.sameValue(result[property], dateTime[property], property);
}

assert.compareArray(actual, expected);
