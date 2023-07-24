// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: Basic tests for equals() calendar handling
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get calendar a.id",
  "get calendar b.id",
];
const actual = [];
const calendar = (id) => {
  const c = {
    dateAdd() {},
    dateFromFields() {},
    dateUntil() {},
    day() {},
    dayOfWeek() {},
    dayOfYear() {},
    daysInMonth() {},
    daysInWeek() {},
    daysInYear() {},
    fields() {},
    inLeapYear() {},
    mergeFields() {},
    month() {},
    monthCode() {},
    monthDayFromFields() {},
    monthsInYear() {},
    weekOfYear() {},
    year() {},
    yearMonthFromFields() {},
    yearOfWeek() {},
  };
  TemporalHelpers.observeProperty(actual, c, "id", id, `calendar ${id}`);
  return c;
};

const mdA = new Temporal.PlainMonthDay(2, 7, calendar("a"));
const mdB = new Temporal.PlainMonthDay(2, 7, calendar("b"));
const mdC = new Temporal.PlainMonthDay(2, 7, calendar("c"), 1974);
actual.splice(0);  // disregard the HasProperty checks done in the constructor

assert.sameValue(mdA.equals(mdC), false, "different year");
assert.compareArray(actual, [], "Should not check calendar");

assert.sameValue(mdA.equals(mdB), false, "different calendar");
assert.compareArray(actual, expected, "Should check calendar");
