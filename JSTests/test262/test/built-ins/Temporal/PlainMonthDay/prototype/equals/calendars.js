// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: Basic tests for equals() calendar handling
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get calendar a.toString",
  "call calendar a.toString",
  "get calendar b.toString",
  "call calendar b.toString",
];
const actual = [];
const calendar = (id) => {
  return TemporalHelpers.toPrimitiveObserver(actual, id, `calendar ${id}`);
};

const mdA = new Temporal.PlainMonthDay(2, 7, calendar("a"));
const mdB = new Temporal.PlainMonthDay(2, 7, calendar("b"));
const mdC = new Temporal.PlainMonthDay(2, 7, calendar("c"), 1974);

assert.sameValue(mdA.equals(mdC), false, "different year");
assert.compareArray(actual, [], "Should not check calendar");

assert.sameValue(mdA.equals(mdB), false, "different calendar");
assert.compareArray(actual, expected, "Should check calendar");
