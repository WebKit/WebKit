// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: The options object passed to calendar.dateUntil has a largestUnit property with its value in the singular form
info: |
    sec-temporal.duration.prototype.round steps 23–27:
      23. Let _unbalanceResult_ be ? UnbalanceDurationRelative(_duration_.[[Years]], _duration_.[[Months]], _duration_.[[Weeks]], _duration_.[[Days]], _largestUnit_, _relativeTo_).
      24. Let _roundResult_ be (? RoundDuration(_unbalanceResult_.[[Years]], _unbalanceResult_.[[Months]], _unbalanceResult_.[[Weeks]], _unbalanceResult_.[[Days]], _duration_.[[Hours]], _duration_.[[Minutes]], _duration_.[[Seconds]], _duration_.[[Milliseconds]], _duration_.[[Microseconds]], _duration_.[[Nanoseconds]], _roundingIncrement_, _smallestUnit_, _roundingMode_, _relativeTo_)).[[DurationRecord]].
      25. Let _adjustResult_ be ? AdjustRoundedDurationDays(_roundResult_.[[Years]], _roundResult_.[[Months]], _roundResult_.[[Weeks]], _roundResult_.[[Days]], _roundResult_.[[Hours]], _roundResult_.[[Minutes]], _roundResult_.[[Seconds]], _roundResult_.[[Milliseconds]], _roundResult_.[[Microseconds]], _roundResult_.[[Nanoseconds]], _roundingIncrement_, _smallestUnit_, _roundingMode_, _relativeTo_).
      26. Let _balanceResult_ be ? BalanceDuration(_adjustResult_.[[Days]], _adjustResult_.[[Hours]], _adjustResult_.[[Minutes]], _adjustResult_.[[Seconds]], _adjustResult_.[[Milliseconds]], _adjustResult_.[[Microseconds]], _adjustResult_.[[Nanoseconds]], _largestUnit_, _relativeTo_).
      27. Let _result_ be ? BalanceDurationRelative(_adjustResult_.[[Years]], _adjustResult_.[[Months]], _adjustResult_.[[Weeks]], _balanceResult_.[[Days]], _largestUnit_, _relativeTo_).
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
    sec-temporal-balancedurationrelative steps 1, 9.m–o, and 9.q.vi–viii:
      1. If _largestUnit_ is not one of *"year"*, *"month"*, or *"week"*, or _years_, _months_, _weeks_, and _days_ are all 0, then
        a. Return ...
      ...
      9. If _largestUnit_ is *"year"*, then
        ...
        m. Let _untilOptions_ be ! OrdinaryObjectCreate(*null*).
        n. Perform ! CreateDataPropertyOrThrow(_untilOptions_, *"largestUnit"*, *"month"*).
        o. Let _untilResult_ be ? CalendarDateUntil(_calendar_, _relativeTo_, _newRelativeTo_, _untilOptions_, _dateUntil_).
        p. ...
        q. Repeat, while abs(_months_) ≥ abs(_oneYearMonths_),
          ...
          vi. Let _untilOptions_ be ! OrdinaryObjectCreate(*null*).
          vii. Perform ! CreateDataPropertyOrThrow(_untilOptions_, *"largestUnit"*, *"month"*).
          viii. Let _untilResult_ be ? CalendarDateUntil(_calendar_, _relativeTo_, _newRelativeTo_, _untilOptions_, _dateUntil_).
    sec-temporal-balanceduration step 3.a:
      3. If _largestUnit_ is one of *"year"*, *"month"*, *"week"*, or *"day"*, then
        a. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _relativeTo_).
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

// Check with smallestUnit nanoseconds but roundingIncrement > 1; each call
// should result in one call to dateUntil() originating from
// AdjustRoundedDurationDays, with largestUnit equal to the largest unit in
// the duration higher than "day".
// Additionally one call with largestUnit: "month" in BalanceDurationRelative
// when the largestUnit given to round() is "year".
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
    years: ["year", "month"],
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
    years: ["month", "month", "month", "month", "month", "month"],
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
    months: [],
    weeks: [],
    days: []
  }
);
