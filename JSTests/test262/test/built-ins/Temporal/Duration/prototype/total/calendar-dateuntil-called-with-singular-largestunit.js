// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: >
  The options object passed to calendar.dateUntil has a largestUnit property
  with its value in the singular form
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// Check the paths that go through NanosecondsToDays: only one call in
// RoundDuration when the unit is a calendar unit. The others all
// have largestUnit: "day" so the difference is taken in ISO calendar space.

const duration = new Temporal.Duration(0, 1, 1, 1, 1, 1, 1, 1, 1, 1);

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, unit) => {
    const relativeTo = new Temporal.ZonedDateTime(0n, "UTC", calendar);
    duration.total({ unit, relativeTo });
  },
  {
    years: ["year"],
    months: ["month"],
    weeks: ["week", "week"],
    days: [],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);
