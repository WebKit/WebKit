// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
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
    sec-temporal-interpretisodatetimeoffset steps 4–10:
      4. If _offsetNanoseconds_ is *null*, or _offset_ is *"ignore"*, then
        a. Let _instant_ be ? BuiltinTimeZoneGetInstantFor(_timeZone_, _dateTime_, _disambiguation_).
        ...
      ...
      6. Assert: _offset_ is *"prefer"* or *"reject"*.
      7. Let _possibleInstants_ be ? GetPossibleInstantsFor(_timeZone_, _dateTime_).
      ...
      9. If _offset_ is *"reject"*, throw a *RangeError* exception.
      10. Let _instant_ be ? DisambiguatePossibleInstants(_possibleInstants_, _timeZone_, _dateTime_, _disambiguation_).
    sec-temporal-totemporalzoneddatetime step 7:
      7. Let _epochNanoseconds_ be ? InterpretISODateTimeOffset(_result_.[[Year]], _result_.[[Month]], _result_.[[Day]], _result_.[[Hour]], _result_.[[Minute]], _result_.[[Second]], _result_.[[Millisecond]], _result_.[[Microsecond]], _result_.[[Nanosecond]], _offsetNanoseconds_, _timeZone_, _disambiguation_, _offset_).
    sec-temporal.zoneddatetime.from step 3:
      3. Return ? ToTemporalZonedDateTime(_item_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const shiftInstant = new Temporal.Instant(3661_001_001_001n);
const tz1 = TemporalHelpers.oneShiftTimeZone(shiftInstant, 2);

// This code path is encountered if offset is `ignore` or `prefer`,
// disambiguation is `earlier` and the shift is a spring-forward change
Temporal.ZonedDateTime.from({
  year: 1970,
  month: 1,
  day: 1,
  hour: 1,
  minute: 1,
  second: 1,
  millisecond: 1,
  microsecond: 1,
  nanosecond: 1,
  timeZone: tz1,
}, { offset: "ignore", disambiguation: "earlier" });

const expected1 = [
  "1970-01-01T01:01:01.001001001",
  "1970-01-01T01:01:01.001000999",
];
assert.compareArray(tz1.getPossibleInstantsForCalledWith, expected1);

const tz2 = TemporalHelpers.oneShiftTimeZone(shiftInstant, 2);

Temporal.ZonedDateTime.from({
  year: 1970,
  month: 1,
  day: 1,
  hour: 1,
  minute: 1,
  second: 1,
  millisecond: 1,
  microsecond: 1,
  nanosecond: 1,
  timeZone: tz2,
}, { offset: "prefer", disambiguation: "earlier" });

const expected2 = [
  "1970-01-01T01:01:01.001001001",
  "1970-01-01T01:01:01.001000999",
];
assert.compareArray(tz2.getPossibleInstantsForCalledWith, expected2);
