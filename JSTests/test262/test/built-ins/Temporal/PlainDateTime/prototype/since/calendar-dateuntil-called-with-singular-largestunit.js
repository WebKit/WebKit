// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.plaindatetime.prototype.since step 14:
      14. Let _diff_ be ? DifferenceISODateTime(_other_.[[ISOYear]], _other_.[[ISOMonth]], _other_.[[ISODay]], _other_.[[ISOHour]], _other_.[[ISOMinute]], _other_.[[ISOSecond]], _other_.[[ISOMillisecond]], _other_.[[ISOMicrosecond]], _other_.[[ISONanosecond]], _dateTime_.[[ISOYear]], _dateTime_.[[ISOMonth]], _dateTime_.[[ISODay]], _dateTime_.[[ISOHour]], _dateTime_.[[ISOMinute]], _dateTime_.[[ISOSecond]], _dateTime_.[[ISOMillisecond]], _dateTime_.[[ISOMicrosecond]], _dateTime_.[[ISONanosecond]], _dateTime_.[[Calendar]], _largestUnit_, _options_).
    sec-temporal-differenceisodatetime steps 9–11:
      9. Let _dateLargestUnit_ be ! LargerOfTwoTemporalUnits(*"day"*, _largestUnit_).
      10. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _dateLargestUnit_).
      11. Let _dateDifference_ be ? CalendarDateUntil(_calendar_, _date1_, _date2_, _untilOptions_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit) => {
    const earlier = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
    const later = new Temporal.PlainDateTime(2001, 6, 3, 13, 35, 57, 988, 655, 322, calendar);
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
