// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.zoneddatetime.prototype.with step 24:
      24. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_dateTimeResult_.[[Year]], [...], _dateTimeResult_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, _disambiguation_, _offset_).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "2005-09-09T01:46:40",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);
  datetime.with({ year: 2005 });
}, expected);
