// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
    When consulting calendar.dateUntil() to calculate the number of months in a
    year, the months property is not accessed on the result Duration
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// One path, through UnbalanceDurationRelative, calls dateUntil() in a loop for
// each year in the duration

const actual = [];
const expected1 = [
  "call dateUntil",
  "call dateUntil",
];
const duration = new Temporal.Duration(0, 12);
TemporalHelpers.observeProperty(actual, duration, "months", 1);

const calendar = TemporalHelpers.calendarDateUntilObservable(actual, duration);
const relativeTo = new Temporal.PlainDateTime(2018, 10, 12, 0, 0, 0, 0, 0, 0, calendar);

const years = new Temporal.Duration(2);
const result1 = years.round({ largestUnit: "months", relativeTo });
TemporalHelpers.assertDuration(result1, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, "result");
assert.compareArray(actual, expected1, "operations");

// There is a second path, through BalanceDurationRelative, that calls
// dateUntil() in a loop for each year in the duration plus one extra time

actual.splice(0); // reset calls for next test
const expected2 = [
  "call dateUntil",
  "call dateUntil",
  "call dateUntil",
];

const months = new Temporal.Duration(0, 24);
const result2 = months.round({ largestUnit: "years", relativeTo });
TemporalHelpers.assertDuration(result2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, "result");
assert.compareArray(actual, expected2, "operations");
