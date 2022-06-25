// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.zoneddatetime.from step 3:
      3. Return ? ToTemporalDateTime(_item_, _options_).
    sec-temporal-totemporalzoneddatetime step 2.j:
      j. Let _result_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInfinityTime();
const datetime = Temporal.ZonedDateTime.from({ year: 2000, month: 5, day: 2, hour: 12, minute: 34, second: 56, millisecond: 987, microsecond: 654, nanosecond: 321, timeZone: "UTC", calendar });

assert.sameValue(datetime.hour, 12, "hour value");
assert.sameValue(datetime.minute, 34, "minute value");
assert.sameValue(datetime.second, 56, "second value");
assert.sameValue(datetime.millisecond, 987, "millisecond value");
assert.sameValue(datetime.microsecond, 654, "microsecond value");
assert.sameValue(datetime.nanosecond, 321, "nanosecond value");
