// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.total
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.duration.prototype.total steps 4 and 10:
      4. Let _relativeTo_ be ? ToRelativeTemporalObject(_options_).
      10. Let _balanceResult_ be ? BalanceDuration(_unbalanceResult_.[[Days]], [...], _unbalanceResult_.[[Nanoseconds]], _unit_, _intermediate_).
    sec-temporal-torelativetemporalobject step 6.d:
      d. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNs_, _timeZone_, *"compatible"*, *"reject"*).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
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
  "2000-01-01T00:00:00", // called once on the input relativeTo if ZonedDateTime
  "2001-02-09T00:00:00", // called once on the intermediate ZonedDateTime with the calendar parts of the Duration added
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const duration = new Temporal.Duration(1, 1, 1, 1, 1, 1, 1);
  duration.total({ unit: 'seconds', relativeTo: { year: 2000, month: 1, day: 1, timeZone } });
}, expected);
