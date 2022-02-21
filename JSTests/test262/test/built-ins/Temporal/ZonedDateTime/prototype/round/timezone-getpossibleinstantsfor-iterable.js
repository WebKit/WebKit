// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.zoneddatetime.prototype.round steps 14, 16, and 20:
      14. Let _instantStart_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _dtStart_, *"compatible"*).
      16. Let _endNs_ be ? AddZonedDateTime(_startNs_, _timeZone_, _zonedDateTime_.[[Calendar]], 0, 0, 0, 1, 0, 0, 0, 0, 0, 0).
      20. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_roundResult_.[[Year]], [...], _roundResult_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, *"compatible"*, *"prefer"*).
    sec-temporal-addzoneddatetime step 8:
      8. Let _intermediateInstant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _intermediateDateTime_, *"compatible"*).
    sec-temporal-builtintimezonegetinstantfor step 1:
      1. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "2001-09-09T00:00:00", // called once on midnight of the input datetime
  "2001-09-10T00:00:00", // called once on the previous value plus one calendar day
  "2001-09-09T02:00:00",  // called once on the rounding result
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);
  datetime.round({ smallestUnit: 'hour' });
}, expected);
