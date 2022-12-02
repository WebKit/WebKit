// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaindate
description: PlainDateTime.toPlainDate is not observably called
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];
const expected = [
  "has timeZone.timeZone",
  "get timeZone.getOffsetNanosecondsFor",
  "call timeZone.getOffsetNanosecondsFor",
];

Object.defineProperty(Temporal.PlainDateTime.prototype, "toPlainDate", {
  get() {
    actual.push("get Temporal.PlainDateTime.prototype.toPlainDate");
    return function() {
      actual.push("call Temporal.PlainDateTime.prototype.toPlainDate");
    };
  },
});

const timeZone = TemporalHelpers.timeZoneObserver(actual, "timeZone", {
  getOffsetNanosecondsFor(instant) {
    assert.sameValue(instant instanceof Temporal.Instant, true, "Instant");
    return 86399_999_999_999;
  },
});

const result = Temporal.Now.plainDate("iso8601", timeZone);
assert.notSameValue(result, undefined);
assert.sameValue(result instanceof Temporal.PlainDate, true);

assert.compareArray(actual, expected);
