// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: Passing a TimeZone to options calls getOffsetNanosecondsFor twice, but not toString
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

const instant = Temporal.Instant.from("1975-02-02T14:25:36.123456Z");
const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  toString: TemporalHelpers.toPrimitiveObserver(actual, "Custom/TimeZone", "name"),
  getOffsetNanosecondsFor(instantArg) {
    assert.sameValue(instantArg.epochNanoseconds, instant.epochNanoseconds);
    return -8735135801679;
  },
});

Object.defineProperty(Temporal.TimeZone, "from", {
  get() {
    actual.push("get Temporal.TimeZone.from");
    return undefined;
  },
});

assert.sameValue(instant.toString({ timeZone }), "1975-02-02T12:00:00.987654321-02:26");
assert.compareArray(actual, expected);
