// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.duration.prototype.round
description: >
    Rounding calculation can result in duration date and time components with
    opposite signs
info: |
  AdjustRoundedDurationDays ( years, months, weeks, days, norm, increment, unit, roundingMode, zonedRelativeTo,
    calendarRec, timeZoneRec, precalculatedPlainDateTime )
  11. Let _adjustedDateDuration_ be ? AddDuration(_years_, _months_, _weeks_, _days_, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    _direction_, 0, 0, 0, 0, 0, 0, *undefined*, _zonedRelativeTo_, _calendarRec_, _timeZoneRec_,
    _precalculatedPlainDateTime_).
  12. Let _roundRecord_ be ! RoundDuration(0, 0, 0, 0, _oneDayLess_, _increment_, _unit_, _roundingMode_).
  13. Return ? CombineDateAndNormalizedTimeDuration(_adjustedDateDuration_,
    _roundRecord_.[[NormalizedDuration]].[[NormalizedTime]]).
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
  
const relativeTo = new Temporal.ZonedDateTime(0n, "UTC", calendar);
  
let d = new Temporal.Duration(1, 0, 0, 10, 25);

assert.throws(RangeError, () => d.round({
    smallestUnit: "nanoseconds",
    roundingIncrement: 5,
    relativeTo,
}));
