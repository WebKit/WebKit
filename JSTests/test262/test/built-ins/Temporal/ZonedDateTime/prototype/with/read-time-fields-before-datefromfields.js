// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.with
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.zoneddatetime.prototype.with step 23:
      23. Let _dateTimeResult_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInfinityTime();
const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
const newDatetime = datetime.with({ year: 2022 });

assert.sameValue(newDatetime.hour, 1, "hour value");
assert.sameValue(newDatetime.minute, 46, "minute value");
assert.sameValue(newDatetime.second, 40, "second value");
assert.sameValue(newDatetime.millisecond, 987, "millisecond value");
assert.sameValue(newDatetime.microsecond, 654, "microsecond value");
assert.sameValue(newDatetime.nanosecond, 321, "nanosecond value");
