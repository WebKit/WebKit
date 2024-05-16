// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: >
  The options object passed to calendar.dateUntil has a largestUnit property
  with its value in the singular form
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// Check with smallestUnit nanoseconds but roundingIncrement > 1; each call
// should result in one call to dateUntil() originating from
// AdjustRoundedDurationDays, with largestUnit equal to the largest unit in
// the duration higher than "day".
// Additionally one call in BalanceDateDurationRelative with the same
// largestUnit.
// Other calls have largestUnit: "day" so the difference is taken in ISO
// calendar space.

const durations = [
  [1, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 1, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 1, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
  [0, 0, 0, 0, 0, 0, 0, 0, 0, 86399_999_999_999],
].map((args) => new Temporal.Duration(...args));

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit, index) => {
    const duration = durations[index];
    const relativeTo = new Temporal.ZonedDateTime(0n, "UTC", calendar);
    duration.round({ largestUnit, roundingIncrement: 2, roundingMode: 'ceil', relativeTo });
  },
  {
    years: ["year"],
    months: ["month"],
    weeks: ["week"],
    days: [],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);

// Check the path that converts months to years and vice versa in
// BalanceDurationRelative and UnbalanceDurationRelative.

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const duration = new Temporal.Duration(5, 60);
    const relativeTo = new Temporal.PlainDateTime(2000, 5, 2, 0, 0, 0, 0, 0, 0, calendar);
    duration.round({ largestUnit, relativeTo });
  },
  {
    years: ["year"],
    months: ["month"],
    weeks: ["week"],
    days: [],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);

// Check the paths that call dateUntil() in RoundDuration. These paths do not
// call dateUntil() in AdjustRoundedDurationDays. Note that there is no
// largestUnit: "month" call in BalanceDurationRelative, because the durations
// have rounded down to 0.

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const duration = new Temporal.Duration(0, 1, 0, 0, 1, 1, 1, 1, 1, 1);
    const relativeTo = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
    duration.round({ largestUnit, smallestUnit: largestUnit, relativeTo });
  }, {
    years: ["year"],
    months: ["month"],
    weeks: ["week", "week"],
    days: []
  }
);
