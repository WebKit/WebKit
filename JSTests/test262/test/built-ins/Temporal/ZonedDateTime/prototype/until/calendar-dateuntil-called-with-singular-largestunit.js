// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.zoneddatetime.prototype.until steps 13–17:
      13. If _largestUnit_ is not one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        ...
        c. Return ...
      14. ...
      15. Let _difference_ be ? DifferenceZonedDateTime(_zonedDateTime_.[[Nanoseconds]], _other_.[[Nanoseconds]], _zonedDateTime_.[[TimeZone]], _zonedDateTime_.[[Calendar]], _largestUnit_).
      16. Let _roundResult_ be ? RoundDuration(_difference_.[[Years]], _difference_.[[Months]], _difference_.[[Weeks]], _difference_.[[Days]], _difference_.[[Hours]], _difference_.[[Minutes]], _difference_.[[Seconds]], _difference_.[[Milliseconds]], _difference_.[[Microseconds]], _difference_.[[Nanoseconds]], _roundingIncrement_, _smallestUnit_, _roundingMode_, _zonedDateTime_).
      17. Let _result_ be ? AdjustRoundedDurationDays(_roundResult_.[[Years]], _roundResult_.[[Months]], _roundResult_.[[Weeks]], _roundResult_.[[Days]], _roundResult_.[[Hours]], _roundResult_.[[Minutes]], _roundResult_.[[Seconds]], _roundResult_.[[Milliseconds]], _roundResult_.[[Microseconds]], _roundResult_.[[Nanoseconds]], _roundingIncrement_, _smallestUnit_, _roundingMode_, _zonedDateTime_).
    sec-temporal-differencezoneddatetime steps 7 and 11:
      7. Let _dateDifference_ be ? DifferenceISODateTime(_startDateTime_.[[ISOYear]], _startDateTime_.[[ISOMonth]], _startDateTime_.[[ISODay]], _startDateTime_.[[ISOHour]], _startDateTime_.[[ISOMinute]], _startDateTime_.[[ISOSecond]], _startDateTime_.[[ISOMillisecond]], _startDateTime_.[[ISOMicrosecond]], _startDateTime_.[[ISONanosecond]], _endDateTime_.[[ISOYear]], _endDateTime_.[[ISOMonth]], _endDateTime_.[[ISODay]], _endDateTime_.[[ISOHour]], _endDateTime_.[[ISOMinute]], _endDateTime_.[[ISOSecond]], _endDateTime_.[[ISOMillisecond]], _endDateTime_.[[ISOMicrosecond]], _endDateTime_.[[ISONanosecond]], _calendar_, _largestUnit_, _options_).
      11. Let _result_ be ? NanosecondsToDays(_timeRemainderNs_, _intermediate_).
    sec-temporal-roundduration steps 5.d and 8.n–p:
      5. If _unit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        ...
        d. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _intermediate_).
      ...
      8. If _unit_ is *"year"*, then
        ...
        n. Let _untilOptions_ be ! OrdinaryObjectCreate(*null*).
        o. Perform ! CreateDataPropertyOrThrow(_untilOptions_, *"largestUnit"*, *"year"*).
        p. Let _timePassed_ be ? CalendarDateUntil(_calendar_, _relativeTo_, _daysLater_, _untilOptions_)
    sec-temporal-adjustroundeddurationdays steps 1 and 9:
      1. If _relativeTo_ does not have an [[InitializedTemporalZonedDateTime]] internal slot; or _unit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*; or _unit_ is *"nanosecond"* and _increment_ is 1, then
        a. Return ...
      ...
      9. Let _adjustedDateDuration_ be ? AddDuration(_years_, _months_, _weeks_, _days_, 0, 0, 0, 0, 0, 0, 0, 0, 0, _direction_, 0, 0, 0, 0, 0, 0, _relativeTo_).
    sec-temporal-addduration step 7.a–g:
      a. Assert: _relativeTo_ has an [[IntializedTemporalZonedDateTime]] internal slot.
      ...
      f. If _largestUnit_ is not one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        ...
      g. Else,
        i. Let _result_ be ? DifferenceZonedDateTime(_relativeTo_.[[Nanoseconds]], _endNs_, _timeZone_, _calendar_, _largestUnit_).
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
  (calendar, largestUnit) => {
    const earlier = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
    const later = new Temporal.ZonedDateTime(1_086_403_661_988_655_322n, "UTC", calendar);
    earlier.until(later, { largestUnit });
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
    earlier.until(later, { largestUnit, roundingIncrement: 2, roundingMode: 'ceil' });
  },
  {
    years: ["year", "year"],
    months: ["month", "month"],
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

// Also check the path that goes through RoundDuration when smallestUnit is
// given

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, smallestUnit) => {
    const earlier = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
    const later = new Temporal.ZonedDateTime(1_086_403_661_988_655_322n, "UTC", calendar);
    earlier.until(later, { smallestUnit });
  },
  {
    years: ["year", "year"],
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
