// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.compare
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.zoneddatetime.compare steps 1–2:
      1. Set _one_ to ? ToTemporalZonedDateTime(_one_).
      2. Set _two_ to ? ToTemporalZonedDateTime(_two_).
    sec-temporal-totemporalzoneddatetime step 7:
      7. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, _disambiguation_, _offset_).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const expected1 = [
  "2000-05-02T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  Temporal.ZonedDateTime.compare(
    { year: 2000, month: 5, day: 2, timeZone },
    { year: 2001, month: 6, day: 3, timeZone: "UTC" },
  );
}, expected1);

// Same, but on the other operand

const expected2 = [
  "2001-06-03T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  Temporal.ZonedDateTime.compare(
    { year: 2000, month: 5, day: 2, timeZone: "UTC" },
    { year: 2001, month: 6, day: 3, timeZone },
  );
}, expected2);
