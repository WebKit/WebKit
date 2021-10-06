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
const expected = [
  "call dateUntil",
  "call dateUntil",
];

const duration = new Temporal.Duration(0, 12);
TemporalHelpers.observeProperty(actual, duration, "months", 1);

const calendar = TemporalHelpers.calendarDateUntilObservable(actual, duration);
const relativeTo = new Temporal.PlainDateTime(2018, 10, 12, 0, 0, 0, 0, 0, 0, calendar);

const years = new Temporal.Duration(2);
const result = years.total({ unit: "months", relativeTo });
assert.sameValue(result, 24, "result");
assert.compareArray(actual, expected, "operations");
