// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.zoneddatetime.prototype.equals step 3:
      3. Set _other_ to ? ToTemporalZonedDateTime(_other_).
    sec-temporal-totemporalzoneddatetime step 7:
      7. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, _disambiguation_, _offset_).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-getpossibleinstantsfor step 2:
      2. Let _list_ be ? IterableToList(_possibleInstants_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

// Not called on the instance's time zone

const expected1 = [];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, timeZone);
  datetime.equals({ year: 2005, month: 6, day: 2, timeZone: "UTC" });
}, expected1);

// Called on the argument's time zone

const expected2 = [
  "2005-06-02T00:00:00",
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC");
  datetime.equals({ year: 2005, month: 6, day: 2, timeZone });
}, expected2);
