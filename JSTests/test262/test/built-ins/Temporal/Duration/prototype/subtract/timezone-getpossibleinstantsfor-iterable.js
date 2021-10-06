// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.subtract
description: An iterable returned from timeZone.getPossibleInstantsFor is consumed after each call
info: |
    sec-temporal.duration.prototype.subtract steps 5–6:
      5. Let _relativeTo_ be ? ToRelativeTemporalObject(_options_).
      6. Let _result_ be ? AddDuration(_duration_.[[Years]], [...], _duration_.[[Nanoseconds]], −_other_.[[Years]], [...], −_other_.[[Nanoseconds]], _relativeTo_).
    sec-temporal-torelativetemporalobject step 6.d:
      d. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], [...], _result_.[[Nanosecond]], _offsetNs_, _timeZone_, *"compatible"*, *"reject"*).
    sec-temporal-interpretisodatetimeoffset step 7:
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
    sec-temporal-addduration steps 7.d–e and 7.g.i:
      d. Let _intermediateNs_ be ? AddZonedDateTime(_relativeTo_.[[Nanoseconds]], _timeZone_, _calendar_, _y1_, [...], _ns1_).
      e. Let _endNs_ be ? AddZonedDateTime(_intermediateNs_, _timeZone_, _calendar_, _y2_, [...], _ns2_).
      [...]
        i. Let _result_ be ? DifferenceZonedDateTime(_relativeTo_.[[Nanoseconds]], _endNs_, _timeZone_, _calendar_, _largestUnit_).
    sec-temporal-differencezoneddatetime step 8:
      8. Let _intermediateNs_ be ? AddZonedDateTime(_ns1_, _timeZone_, _calendar_, _dateDifference_.[[Years]], _dateDifference_.[[Months]], _dateDifference_.[[Weeks]], 0, 0, 0, 0, 0, 0, 0).
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
  "2000-01-01T09:00:00", // called once on the input relativeTo object
  "2001-01-01T09:00:00", // called once on relativeTo plus the receiver
  "1999-12-01T09:00:00", // called once on relativeTo plus the receiver minus the argument
  "1999-12-01T09:00:00", // called once on relativeTo plus the years, months, and weeks from the difference of relativeTo minus endNs
];

TemporalHelpers.checkTimeZonePossibleInstantsIterable((timeZone) => {
  const duration1 = new Temporal.Duration(1);
  const duration2 = new Temporal.Duration(0, 13);
  duration1.subtract(duration2, { relativeTo: { year: 2000, month: 1, day: 1, hour: 9, timeZone } });
}, expected);
