// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.zoneddatetime.from step 3:
      3. Return ? ToTemporalZonedDateTime(_item_, _options_).
    sec-temporal-totemporalzoneddatetime step 7:
      7. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, _disambiguation_, _offset_).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "2000-05-02T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, timeZone });
}, expected);
