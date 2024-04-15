// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    When consulting calendar.dateUntil() to balance or unbalance a duration, no
    properties are accessed on the result Duration
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const actual = [];

class CalendarDateUntilObservable extends Temporal.Calendar {
  dateUntil(...args) {
    actual.push("call dateUntil");
    const returnValue = super.dateUntil(...args);
    TemporalHelpers.observeProperty(actual, returnValue, "years", Infinity);
    TemporalHelpers.observeProperty(actual, returnValue, "months", Infinity);
    TemporalHelpers.observeProperty(actual, returnValue, "weeks", Infinity);
    TemporalHelpers.observeProperty(actual, returnValue, "days", Infinity);
    return returnValue;
  }
}

const calendar = new CalendarDateUntilObservable("iso8601");
const relativeTo = new Temporal.PlainDate(2018, 10, 12, calendar);

const expected = [
  "call dateUntil",  // UnbalanceDateDurationRelative
  "call dateUntil",  // BalanceDateDurationRelative
];

const years = new Temporal.Duration(2);
const result = years.round({ largestUnit: "months", relativeTo });
TemporalHelpers.assertDuration(result, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, "result");
assert.compareArray(actual, expected, "operations");
