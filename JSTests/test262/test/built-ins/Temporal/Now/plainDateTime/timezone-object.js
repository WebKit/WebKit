// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.plaindatetime
description: Observable interactions with the provided timezone-like object
includes: [compareArray.js, temporalHelpers.js]
features: [BigInt, Proxy, Temporal]
---*/
const actual = [];

const expected = [
  'has timeZone.timeZone',
  'get timeZone.timeZone',
  'has nestedTimeZone.timeZone',
  'get nestedTimeZone.getOffsetNanosecondsFor',
  'call nestedTimeZone.getOffsetNanosecondsFor'
];

const nestedTimeZone = TemporalHelpers.timeZoneObserver(actual, "nestedTimeZone", {
  getOffsetNanosecondsFor(instant) {
    assert.sameValue(
      instant instanceof Temporal.Instant,
      true,
      'The result of evaluating (instant instanceof Temporal.Instant) is expected to be true'
    );

    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
});

const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor(instant) {
    assert.sameValue(
      instant instanceof Temporal.Instant,
      true,
      'The result of evaluating (instant instanceof Temporal.Instant) is expected to be true'
    );

    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
});
timeZone.timeZone = nestedTimeZone;

Object.defineProperty(Temporal.TimeZone, 'from', {
  get() {
    actual.push('get Temporal.TimeZone.from');
    return undefined;
  }
});

Temporal.Now.plainDateTime('iso8601', timeZone);
assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');
