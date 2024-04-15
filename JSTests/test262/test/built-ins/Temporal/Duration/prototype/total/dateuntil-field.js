// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
    When consulting calendar.dateUntil() to calculate the number of months in a
    year, the months property is not accessed on the result Duration
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];

class CalendarDateUntilObservable extends Temporal.Calendar {
  dateUntil(...args) {
    actual.push("call dateUntil");
    const returnValue = super.dateUntil(...args);
    TemporalHelpers.observeProperty(actual, returnValue, "months", Infinity);
    return returnValue;
  }
}

const calendar = new CalendarDateUntilObservable("iso8601");
const relativeTo = new Temporal.PlainDate(2018, 10, 12, calendar);

const expected = [
  "call dateUntil",
];

const years = new Temporal.Duration(2);
const result = years.total({ unit: "months", relativeTo });
assert.sameValue(result, 24, "result");
assert.compareArray(actual, expected, "operations");
