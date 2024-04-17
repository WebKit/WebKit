// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.zoneddatetime.prototype.until
description: >
    Rounding calculation in difference method can result in duration date and
    time components with opposite signs
info: |
  DifferenceTemporalZonedDateTime ( operation, zonedDateTime, other, options )
  17. If _roundingGranularityIsNoop_ is *false*, then
    ...
    e. Let _adjustResult_ be ? AdjustRoundedDurationDays(_roundResult_.[[Years]], _roundResult_.[[Months]],
        _roundResult_.[[Weeks]], _days_, _daysResult_.[[NormalizedTime]], _settings_.[[RoundingIncrement]],
        _settings_.[[SmallestUnit]], _settings_.[[RoundingMode]], _zonedDateTime_, _calendarRec_, _timeZoneRec_,
        _precalculatedPlainDateTime_).
    f. Let _balanceResult_ be ? BalanceDateDurationRelative(_adjustResult_.[[Years]], _adjustResult_.[[Months]],
        _adjustResult_.[[Weeks]], _adjustResult_.[[Days]], _settings_.[[LargestUnit]], _settings_.[[SmallestUnit]],
        _plainRelativeTo_, _calendarRec_).
    g. Set _result_ to ? CombineDateAndNormalizedTimeDuration(_balanceResult_, _adjustResult_.[[NormalizedTime]]).
features: [Temporal]
---*/

// Based on a test case by André Bargull

const calendar = new class extends Temporal.Calendar {
    #dateUntil = 0;
  
    dateUntil(one, two, options) {
        let result = super.dateUntil(one, two, options);
        if (++this.#dateUntil === 2) {
            result = result.negated();
        }
        return result;
    }
}("iso8601");

const oneDay = 86400_000_000_000;
const start = new Temporal.ZonedDateTime(0n, "UTC", calendar);
const end = new Temporal.ZonedDateTime(BigInt(500.5 * oneDay), "UTC", calendar);

assert.throws(RangeError, () => start.until(end, {
    largestUnit: "years",
    smallestUnit: "hours",
}));
