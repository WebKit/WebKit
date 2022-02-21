// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.equals
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.zoneddatetime.prototype.equals step 3:
      3. Set _other_ to ? ToTemporalZonedDateTime(_other_).
    sec-temporal-totemporalzoneddatetime step 2.j:
      j. Let _result_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInfinityTime();
const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);
const result = datetime.equals({ year: 2001, month: 9, day: 9, hour: 1, minute: 46, second: 40, millisecond: 987, microsecond: 654, nanosecond: 321, timeZone: "UTC", calendar });

assert(result, "time fields are not modified");
