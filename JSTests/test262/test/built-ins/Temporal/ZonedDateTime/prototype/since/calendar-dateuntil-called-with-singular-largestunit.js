// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: >
  The options object passed to calendar.dateUntil has a largestUnit property
  with its value in the singular form
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
    const later = new Temporal.ZonedDateTime(1_086_403_661_988_655_322n, "UTC", calendar);
    later.since(earlier, { largestUnit });
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

// Additionally check the path that goes through AdjustRoundedDurationDays

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.ZonedDateTime(-31536000_000_000_000n /* = -365 days */, "UTC", calendar);
    const later = new Temporal.ZonedDateTime(86_399_999_999_999n, "UTC", calendar);
    later.since(earlier, { largestUnit, roundingIncrement: 2, roundingMode: 'ceil' });
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

// Also check the path that goes through RoundDuration when smallestUnit is
// given

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, smallestUnit) => {
    const earlier = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
    const later = new Temporal.ZonedDateTime(1_086_403_661_988_655_322n, "UTC", calendar);
    later.since(earlier, { smallestUnit });
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
