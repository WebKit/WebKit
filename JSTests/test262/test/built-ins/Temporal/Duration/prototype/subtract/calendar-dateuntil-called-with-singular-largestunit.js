// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.duration.prototype.subtract step 6:
      6. Let _result_ be ? AddDuration(_duration_.[[Years]], _duration_.[[Months]], _duration_.[[Weeks]], _duration_.[[Days]], _duration_.[[Hours]], _duration_.[[Minutes]], _duration_.[[Seconds]], _duration_.[[Milliseconds]], _duration_.[[Microseconds]], _duration_.[[Nanoseconds]], −_other_.[[Years]], −_other_.[[Months]], −_other_.[[Weeks]], −_other_.[[Days]], −_other_.[[Hours]], −_other_.[[Minutes]], −_other_.[[Seconds]], −_other_.[[Milliseconds]], −_other_.[[Microseconds]], −_other_.[[Nanoseconds]], _relativeTo_).
    sec-temporal-addduration steps 6-7:
      6. If _relativeTo_ has an [[InitializedTemporalPlainDateTime]] internal slot, then
        ...
        j. Let _dateLargestUnit_ be ! LargerOfTwoTemporalUnits(*"day"*, _largestUnit_).
        k. Let _differenceOptions_ be ! OrdinaryObjectCreate(*null*).
        l. Perform ! CreateDataPropertyOrThrow(_differenceOptions_, *"largestUnit"*, _dateLargestUnit_).
        m. Let _dateDifference_ be ? CalendarDateUntil(_calendar_, _datePart_, _end_, _differenceOptions_).
        ...
      7. Else,
        a. Assert: _relativeTo_ has an [[IntializedTemporalZonedDateTime]] internal slot.
        ...
        f. If _largestUnit_ is not one of *"year"*, *"month"*, *"week"*, or *"day"*, then
          ...
        g. Else,
          i. Let _result_ be ? DifferenceZonedDateTime(_relativeTo_.[[Nanoseconds]], _endNs_, _timeZone_, _calendar_, _largestUnit_).
    sec-temporal-differencezoneddatetime steps 7 and 11:
      7. Let _dateDifference_ be ? DifferenceISODateTime(_startDateTime_.[[ISOYear]], _startDateTime_.[[ISOMonth]], _startDateTime_.[[ISODay]], _startDateTime_.[[ISOHour]], _startDateTime_.[[ISOMinute]], _startDateTime_.[[ISOSecond]], _startDateTime_.[[ISOMillisecond]], _startDateTime_.[[ISOMicrosecond]], _startDateTime_.[[ISONanosecond]], _endDateTime_.[[ISOYear]], _endDateTime_.[[ISOMonth]], _endDateTime_.[[ISODay]], _endDateTime_.[[ISOHour]], _endDateTime_.[[ISOMinute]], _endDateTime_.[[ISOSecond]], _endDateTime_.[[ISOMillisecond]], _endDateTime_.[[ISOMicrosecond]], _endDateTime_.[[ISONanosecond]], _calendar_, _largestUnit_, _options_).
      11. Let _result_ be ? NanosecondsToDays(_timeRemainderNs_, _intermediate_).
    sec-temporal-nanosecondstodays step 11:
      11. 1. Let _dateDifference_ be ? DifferenceISODateTime(_startDateTime_.[[ISOYear]], _startDateTime_.[[ISOMonth]], _startDateTime_.[[ISODay]], _startDateTime_.[[ISOHour]], _startDateTime_.[[ISOMinute]], _startDateTime_.[[ISOSecond]], _startDateTime_.[[ISOMillisecond]], _startDateTime_.[[ISOMicrosecond]], _startDateTime_.[[ISONanosecond]], _endDateTime_.[[ISOYear]], _endDateTime_.[[ISOMonth]], _endDateTime_.[[ISODay]], _endDateTime_.[[ISOHour]], _endDateTime_.[[ISOMinute]], _endDateTime_.[[ISOSecond]], _endDateTime_.[[ISOMillisecond]], _endDateTime_.[[ISOMicrosecond]], _endDateTime_.[[ISONanosecond]], _relativeTo_.[[Calendar]], *"day"*).
    sec-temporal-differenceisodatetime steps 9–11:
      9. Let _dateLargestUnit_ be ! LargerOfTwoTemporalUnits(*"day"*, _largestUnit_).
      10. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _dateLargestUnit_).
      11. Let _dateDifference_ be ? CalendarDateUntil(_calendar_, _date1_, _date2_, _untilOptions_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit, index) => {
    const one = new Temporal.Duration(...[...Array(index).fill(0), ...Array(10 - index).fill(1)]);
    const two = new Temporal.Duration(...[...Array(index).fill(0), ...Array(10 - index).fill(2)]);
    const relativeTo = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);
    two.subtract(one, { relativeTo });
  },
  {
    years: ["year"],
    months: ["month"],
    weeks: ["week"],
    days: ["day"],
    hours: ["day"],
    minutes: ["day"],
    seconds: ["day"],
    milliseconds: ["day"],
    microseconds: ["day"],
    nanoseconds: ["day"]
  }
);

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, largestUnit, index) => {
    const one = new Temporal.Duration(...[...Array(index).fill(0), ...Array(10 - index).fill(1)]);
    const two = new Temporal.Duration(...[...Array(index).fill(0), ...Array(10 - index).fill(2)]);
    const relativeTo = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
    two.subtract(one, { relativeTo });
  },
  {
    years: ["year", "day"],
    months: ["month", "day"],
    weeks: ["week", "day"],
    days: ["day", "day"],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);
