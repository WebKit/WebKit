// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.timezone.prototype.getinstantfor
description: Negative time fields are balanced upwards
info: |
    sec-temporal-balancetime steps 3–14:
      3. Set _microsecond_ to _microsecond_ + floor(_nanosecond_ / 1000).
      4. Set _nanosecond_ to _nanosecond_ modulo 1000.
      5. Set _millisecond_ to _millisecond_ + floor(_microsecond_ / 1000).
      6. Set _microsecond_ to _microsecond_ modulo 1000.
      7. Set _second_ to _second_ + floor(_millisecond_ / 1000).
      8. Set _millisecond_ to _millisecond_ modulo 1000.
      9. Set _minute_ to _minute_ + floor(_second_ / 60).
      10. Set _second_ to _second_ modulo 60.
      11. Set _hour_ to _hour_ + floor(_minute_ / 60).
      12. Set _minute_ to _minute_ modulo 60.
      13. Let _days_ be floor(_hour_ / 24).
      14. Set _hour_ to _hour_ modulo 24.
    sec-temporal-addtime step 8:
      8. Return ? BalanceTime(_hour_, _minute_, _second_, _millisecond_, _microsecond_, _nanosecond_).
    sec-temporal-adddatetime step 1:
      1. Let _timeResult_ be ? AddTime(_hour_, _minute_, _second_, _millisecond_, _microsecond_, _nanosecond_, _hours_, _minutes_, _seconds_, _milliseconds_, _microseconds_, _nanoseconds_).
    sec-temporal-builtintimezonegetinstantfor step 13.a:
      a. Let _earlier_ be ? AddDateTime(_dateTime_.[[ISOYear]], _dateTime_.[[ISOMonth]], _dateTime_.[[ISODay]], _dateTime_.[[ISOHour]], _dateTime_.[[ISOMinute]], _dateTime_.[[ISOSecond]], _dateTime_.[[ISOMillisecond]], _dateTime_.[[ISOMicrosecond]], _dateTime_.[[ISONanosecond]], 0, 0, 0, 0, 0, 0, 0, 0, 0, −_nanoseconds_, *"constrain"*).
    sec-temporal.timezone.prototype.getinstantfor step 6:
      6. Return ? BuiltinTimeZoneGetInstantFor(_timeZone_, _dateTime_, _disambiguation_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const shiftInstant = new Temporal.Instant(3661_001_001_001n);
const tz = TemporalHelpers.oneShiftTimeZone(shiftInstant, 2);
const datetime = new Temporal.PlainDateTime(1970, 1, 1, 1, 1, 1, 1, 1, 1);

// This code path is encountered if disambiguation is `earlier` and the shift is
// a spring-forward change
tz.getInstantFor(datetime, { disambiguation: "earlier" });

const expected = [
  "1970-01-01T01:01:01.001001001",
  "1970-01-01T01:01:01.001000999",
];
assert.compareArray(tz.getPossibleInstantsForCalledWith, expected);
