// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.duration.compare steps 4–6:
      4. Let _relativeTo_ be ? ToRelativeTemporalObject(_options_).
      5. Let _shift1_ be ! CalculateOffsetShift(_relativeTo_, _one_.[[Years]], [...], _one_.[[Nanoseconds]]).
      6. Let _shift2_ be ! CalculateOffsetShift(_relativeTo_, _two_.[[Years]], [...], _two_.[[Nanoseconds]]).
    sec-temporal-torelativetemporalobject step 6.d:
      d. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNs_, _timeZone_, *"compatible"*, *"reject"*).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-calculateoffsetshift step 4:
      4. Let _after_ be ? AddZonedDateTime(_relativeTo_.[[Nanoseconds]], _relativeTo_.[[TimeZone]], _relativeTo_.[[Calendar]], _y_, [...], _ns_).
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
  "2001-01-01T00:00:00", // called once on relativeTo plus the first operand
  "2001-02-01T00:00:00", // called once on relativeTo plus the second operand
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const duration1 = new Temporal.Duration(1);
  const duration2 = new Temporal.Duration(0, 13);
  Temporal.Duration.compare(duration1, duration2, { relativeTo: { year: 2000, month: 1, day: 1, timeZone } });
}, expected);
