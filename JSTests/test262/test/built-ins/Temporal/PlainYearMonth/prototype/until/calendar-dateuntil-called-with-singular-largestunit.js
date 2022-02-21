// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.plainyearmonth.prototype.until steps 20–21:
      20. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _largestUnit_).
      21. Let _result_ be ? CalendarDateUntil(_calendar_, _thisDate_, _otherDate_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.PlainYearMonth(2000, 5, calendar);
    const later = new Temporal.PlainYearMonth(2001, 6, calendar);
    earlier.until(later, { largestUnit });
  },
  {
    years: ["year"],
    months: ["month"]
  }
);
