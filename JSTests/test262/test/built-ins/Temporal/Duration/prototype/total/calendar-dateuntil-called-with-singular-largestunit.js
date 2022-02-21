// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.duration.prototype.total steps 7–11:
        7. Let _unbalanceResult_ be ? UnbalanceDurationRelative(_duration_.[[Years]], _duration_.[[Months]], _duration_.[[Weeks]], _duration_.[[Days]], _unit_, _relativeTo_).
        ...
        10. Let _balanceResult_ be ? BalanceDuration(_unbalanceResult_.[[Days]], _unbalanceResult_.[[Hours]], _unbalanceResult_.[[Minutes]], _unbalanceResult_.[[Seconds]], _unbalanceResult_.[[Milliseconds]], _unbalanceResult_.[[Microseconds]], _unbalanceResult_.[[Nanoseconds]], _unit_, _intermediate_).
        11. Let _roundResult_ be ? RoundDuration(_unbalanceResult_.[[Years]], _unbalanceResult_.[[Months]], _unbalanceResult_.[[Weeks]], _balanceResult_.[[Days]], _balanceResult_.[[Hours]], _balanceResult_.[[Minutes]], _balanceResult_.[[Seconds]], _balanceResult_.[[Milliseconds]], _balanceResult_.[[Microseconds]], _balanceResult_.[[Nanoseconds]], 1, _unit_, *"trunc"*, _relativeTo_).
    sec-temporal-unbalancedurationrelative steps 1 and 9.d.iii–v:
      1. If _largestUnit_ is *"year"*, or _years_, _months_, _weeks_, and _days_ are all 0, then
        a. Return ...
      ...
      9. If _largestUnit_ is *"month"*, then
        ...
        d. Repeat, while abs(_years_) > 0,
          ...
          iii. Let _untilOptions_ be ! OrdinaryObjectCreate(*null*).
          iv. Perform ! CreateDataPropertyOrThrow(_untilOptions_, *"largestUnit"*, *"month"*).
          v. Let _untilResult_ be ? CalendarDateUntil(_calendar_, _relativeTo_, _newRelativeTo_, _untilOptions_, _dateUntil_).
    sec-temporal-balanceduration step 3.a:
      3. If _largestUnit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        a. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _relativeTo_).
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
    sec-temporal-nanosecondstodays step 11:
      11. 1. Let _dateDifference_ be ? DifferenceISODateTime(_startDateTime_.[[ISOYear]], _startDateTime_.[[ISOMonth]], _startDateTime_.[[ISODay]], _startDateTime_.[[ISOHour]], _startDateTime_.[[ISOMinute]], _startDateTime_.[[ISOSecond]], _startDateTime_.[[ISOMillisecond]], _startDateTime_.[[ISOMicrosecond]], _startDateTime_.[[ISONanosecond]], _endDateTime_.[[ISOYear]], _endDateTime_.[[ISOMonth]], _endDateTime_.[[ISODay]], _endDateTime_.[[ISOHour]], _endDateTime_.[[ISOMinute]], _endDateTime_.[[ISOSecond]], _endDateTime_.[[ISOMillisecond]], _endDateTime_.[[ISOMicrosecond]], _endDateTime_.[[ISONanosecond]], _relativeTo_.[[Calendar]], *"day"*).
    sec-temporal-differenceisodatetime steps 9–11:
      9. Let _dateLargestUnit_ be ! LargerOfTwoTemporalUnits(*"day"*, _largestUnit_).
      10. Let _untilOptions_ be ? MergeLargestUnitOption(_options_, _dateLargestUnit_).
      11. Let _dateDifference_ be ? CalendarDateUntil(_calendar_, _date1_, _date2_, _untilOptions_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

// Check the paths that go through NanosecondsToDays: one call to dateUntil() in
// BalanceDuration and one in RoundDuration with largestUnit: "day" when the
// unit is "year", "month", "week", or "day", and one extra call with
// largestUnit: "year" in RoundDuration when the unit is "year".

const duration = new Temporal.Duration(0, 1, 1, 1, 1, 1, 1, 1, 1, 1);

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, unit) => {
    const relativeTo = new Temporal.ZonedDateTime(0n, "UTC", calendar);
    duration.total({ unit, relativeTo });
  },
  {
    years: ["day", "day", "year"],
    months: ["day", "day"],
    weeks: ["day", "day"],
    days: ["day", "day"],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);

// Check the path that converts years to months in UnbalanceDurationRelative.

TemporalHelpers.checkCalendarDateUntilLargestUnitSingular(
  (calendar, unit) => {
    const duration = new Temporal.Duration(5);
    const relativeTo = new Temporal.PlainDateTime(2000, 5, 2, 0, 0, 0, 0, 0, 0, calendar);
    duration.total({ unit, relativeTo });
  },
  {
    years: ["year"],
    months: ["month", "month", "month", "month", "month"],
    weeks: [],
    days: [],
    hours: [],
    minutes: [],
    seconds: [],
    milliseconds: [],
    microseconds: [],
    nanoseconds: []
  }
);
