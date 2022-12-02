// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.equals
description: equals() takes the calendar into account
includes: [compareArray.js]
features: [Temporal]
---*/

const actual = [];
class CustomCalendar extends Temporal.Calendar {
  constructor(id) {
    super("iso8601");
    this._id = id;
  }
  toString() {
    actual.push(this._id);
    return this._id;
  }
}

const sharedCalendar = new CustomCalendar("a");
const ym1 = new Temporal.PlainYearMonth(2000, 1, sharedCalendar, 1);
const ym2 = new Temporal.PlainYearMonth(2000, 1, sharedCalendar, 1);
assert.sameValue(ym1.equals(ym2), true);
assert.compareArray(actual, [], "should not call toString if objects are equal");

const ym3 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("b"), 1);
const ym4 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("c"), 2);
assert.sameValue(ym3.equals(ym4), false);
assert.compareArray(actual, [], "should not call toString if ISO dates differ");

const ym5 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("d"), 1);
const ym6 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("e"), 1);
assert.sameValue(ym5.equals(ym6), false);
assert.compareArray(actual, ["d", "e"], "order of operations");

actual.splice(0); // empty it for the next check
const ym7 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("f"), 1);
const ym8 = new Temporal.PlainYearMonth(2000, 1, new CustomCalendar("f"), 1);
assert.sameValue(ym7.equals(ym8), true);
assert.compareArray(actual, ["f", "f"], "order of operations");
