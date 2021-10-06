// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getplaindatetimefor
description: getOffsetNanosecondsFor is called by getPlainDateTimeFor
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456789Z");
const timeZone = new Proxy({
  getOffsetNanosecondsFor(instantArg) {
    actual.push("call timeZone.getOffsetNanosecondsFor");
    assert.sameValue(instantArg, instant);
    return 9876543210123;
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

const result = Temporal.TimeZone.prototype.getPlainDateTimeFor.call(timeZone, instant);
TemporalHelpers.assertPlainDateTime(result, 1975, 2, "M02", 2, 17, 10, 12, 666, 666, 912);
assert.compareArray(actual, expected);
