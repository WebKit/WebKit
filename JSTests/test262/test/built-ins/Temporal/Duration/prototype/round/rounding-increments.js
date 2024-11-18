// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: Test various rounding increments.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const d = new Temporal.Duration(5, 5, 5, 5, 5, 5, 5, 5, 5, 5);
const relativeTo = new Temporal.PlainDate(2020, 1, 1);

// Rounds to an increment of hours
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "hours",
    roundingIncrement: 3,
    relativeTo
}), 5, 6, 0, 10, 6, 0, 0, 0, 0, 0);

// Rounds to an increment of minutes
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "minutes",
    roundingIncrement: 30,
    relativeTo
}), 5, 6, 0, 10, 5, 0, 0, 0, 0, 0);

// Rounds to an increment of seconds
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "seconds",
    roundingIncrement: 15,
    relativeTo
}), 5, 6, 0, 10, 5, 5, 0, 0, 0, 0);

// Rounds to an increment of milliseconds
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "milliseconds",
    roundingIncrement: 10,
    relativeTo
}), 5, 6, 0, 10, 5, 5, 5, 10, 0, 0);

// Rounds to an increment of microseconds
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "microseconds",
    roundingIncrement: 10,
    relativeTo
}), 5, 6, 0, 10, 5, 5, 5, 5, 10, 0);

// Rounds to an increment of nanoseconds
TemporalHelpers.assertDuration(d.round({
    smallestUnit: "nanoseconds",
    roundingIncrement: 10,
    relativeTo
}), 5, 6, 0, 10, 5, 5, 5, 5, 5, 10);
