// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.equals
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.plaindatetime.prototype.equals step 3:
      3. Set _other_ to ? ToTemporalDateTime(_other_).
    sec-temporal-totemporaldatetime step 2.e:
      e. Let _result_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInfinityTime();
const datetime = new Temporal.PlainDateTime(2021, 3, 31, 12, 34, 56, 987, 654, 321, calendar);
const result = datetime.equals({ year: 2021, month: 3, day: 31, hour: 12, minute: 34, second: 56, millisecond: 987, microsecond: 654, nanosecond: 321, calendar });

assert(result, "time fields are not modified");
