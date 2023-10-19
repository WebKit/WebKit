// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.since
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.plaindate.prototype.since steps 13–14:
      13. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _largestUnit_).
      14. Let _result_ be ? CalendarDateUntil(_temporalDate_.[[Calendar]], _other_, _temporalDate_, _untilOptions_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.PlainDate(2000, 5, 2, calendar);
    const later = new Temporal.PlainDate(2001, 6, 3, calendar);
    later.since(earlier, { largestUnit });
  },
  {
    years: ["year"],
    months: ["month"],
    weeks: ["week"],
    days: []
  }
);
