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
  'has timeZone.getOffsetNanosecondsFor',
  'has timeZone.getPossibleInstantsFor',
  'has timeZone.id',
  'get timeZone.getOffsetNanosecondsFor',
  'call timeZone.getOffsetNanosecondsFor'
];

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

Object.defineProperty(Temporal.TimeZone, 'from', {
  get() {
    actual.push('get Temporal.TimeZone.from');
    return undefined;
  }
});

Temporal.Now.plainDateTime('iso8601', timeZone);
assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');
