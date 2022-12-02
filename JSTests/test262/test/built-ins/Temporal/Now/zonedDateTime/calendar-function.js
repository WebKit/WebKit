// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Behavior when provided calendar value is a function
includes: [compareArray.js, temporalHelpers.js]
features: [BigInt, Proxy, Temporal]
---*/
const actual = [];

const expected = [
  'has timeZone.timeZone'
];

const calendar = function() {};

const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor(instant) {
    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
});

Object.defineProperty(Temporal.Calendar, 'from', {
  get() {
    actual.push('get Temporal.Calendar.from');
    return undefined;
  }
});

const result = Temporal.Now.zonedDateTime(calendar, timeZone);

assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');

for (const property of ['hour', 'minute', 'second', 'millisecond', 'microsecond', 'nanosecond']) {
  assert.sameValue(result[property], 0, 'The value of result[property] is expected to be 0');
}
