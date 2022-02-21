// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.now.zoneddatetime
description: Observable interactions with the provided timezone-like object
includes: [compareArray.js]
features: [BigInt, Proxy, Temporal]
---*/
const actual = [];

const expected = [
  'has timeZone.timeZone',
  'get timeZone.timeZone',
  'has nestedTimeZone.timeZone'
];

const nestedTimeZone = new Proxy({
  getOffsetNanosecondsFor(instant) {
    actual.push('call nestedTimeZone.getOffsetNanosecondsFor');

    assert.sameValue(
      instant instanceof Temporal.Instant,
      true,
      'The result of evaluating (instant instanceof Temporal.Instant) is expected to be true'
    );

    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
}, {
  has(target, property) {
    actual.push(`has nestedTimeZone.${String(property)}`);
    return property in target;
  },

  get(target, property) {
    actual.push(`get nestedTimeZone.${String(property)}`);
    return target[property];
  }
});

const timeZone = new Proxy({
  timeZone: nestedTimeZone,

  getOffsetNanosecondsFor(instant) {
    actual.push('call timeZone.getOffsetNanosecondsFor');

    assert.sameValue(
      instant instanceof Temporal.Instant,
      true,
      'The result of evaluating (instant instanceof Temporal.Instant) is expected to be true'
    );

    return -Number(instant.epochNanoseconds % 86400000000000n);
  }
}, {
  has(target, property) {
    actual.push(`has timeZone.${property}`);
    return property in target;
  },

  get(target, property) {
    actual.push(`get timeZone.${property}`);
    return target[property];
  }
});

Object.defineProperty(Temporal.TimeZone, 'from', {
  get() {
    actual.push('get Temporal.TimeZone.from');
    return undefined;
  }
});

Temporal.Now.zonedDateTime('iso8601', timeZone);
assert.compareArray(actual, expected, 'The value of actual is expected to equal the value of expected');
