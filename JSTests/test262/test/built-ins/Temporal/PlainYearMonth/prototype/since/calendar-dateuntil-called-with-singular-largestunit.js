// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.plainyearmonth.prototype.since steps 21–22:
      21. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _largestUnit_).
      22. Let _result_ be ? CalendarDateUntil(_calendar_, _thisDate_, _otherDate_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.PlainYearMonth(2000, 5, calendar);
    const later = new Temporal.PlainYearMonth(2001, 6, calendar);
    later.since(earlier, { largestUnit });
  },
  {
    years: ["year"],
    months: ["month"]
  }
);
