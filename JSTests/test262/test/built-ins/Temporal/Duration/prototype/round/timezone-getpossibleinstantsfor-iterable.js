// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.round
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.duration.prototype.round steps 19, 21, and 24:
      19. Let _relativeTo_ be ? ToRelativeTemporalObject(_options_).
      21. Let _roundResult_ be ? RoundDuration(_unbalanceResult_.[[Years]], [...], _unbalanceResult_.[[Days]], _duration_.[[Hours]], [...], _duration_.[[Nanoseconds]], _roundingIncrement_, _smallestUnit_, _roundingMode_, _relativeTo_).
      24. If _relativeTo_ has an [[InitializedTemporalZonedDateTime]] internal slot, then
        a. Set _relativeTo_ to ? MoveRelativeZonedDateTime(_relativeTo_, _balanceResult_.[[Years]], _balanceResult_.[[Months]], _balanceResult_.[[Weeks]], 0).
    sec-temporal-torelativetemporalobject step 6.d:
      d. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNs_, _timeZone_, *"compatible"*, *"reject"*).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-roundduration step 5.c–d:
      c. If _zonedRelativeTo_ is not *undefined*, then
        i. Let _intermediate_ be ? MoveRelativeZonedDateTime(_zonedRelativeTo_, _years_, _months_, _weeks_, _days_).
      d. Let _result_ be ? NanosecondsToDays(_nanoseconds_, _intermediate_).
    sec-temporal-moverelativezoneddatetime step 1:
      1. Let _intermediateNs_ be ? AddZonedDateTime(_zonedDateTime_.[[Nanoseconds]], _zonedDateTime_.[[TimeZone]], _zonedDateTime_.[[Calendar]], _years_, _months_, _weeks_, _days_, 0, 0, 0, 0, 0, 0).
    sec-temporal-nanosecondstodays step 13:
      13. Let _intermediateNs_ be ? AddZonedDateTime(_startNs_, _relativeTo_.[[TimeZone]], _relativeTo_.[[Calendar]], 0, 0, 0, _days_, 0, 0, 0, 0, 0, 0).
    sec-temporal-addzoneddatetime step 8:
      8. Let _intermediateInstant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _intermediateDateTime_, *"compatible"*).
    sec-temporal-builtintimezonegetinstantfor step 1:
      1. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "2000-01-01T00:00:00", // called once on the input relativeTo object
  "2001-02-09T00:00:00", // called once on relativeTo plus years, months, weeks, days from the receiver
  "2001-02-10T00:00:00", // called once on the previous value plus the calendar days difference between that and the time part of the duration
  "2001-02-01T00:00:00", // called once on relativeTo plus the years, months, and weeks part of the balance result
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const duration = new Temporal.Duration(1, 1, 1, 1, 1, 1, 1);
  duration.round({ smallestUnit: 'months', relativeTo: { year: 2000, month: 1, day: 1, timeZone } });
}, expected);
