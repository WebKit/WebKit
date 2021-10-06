// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.compare
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.plaindatetime.compare steps 1–2:
      1. Set _one_ to ? ToTemporalDateTime(_one_).
      2. Set _two_ to ? ToTemporalDateTime(_two_).
    sec-temporal-totemporaldatetime step 2.e:
      e. Let _result_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInfinityTime();
const result = Temporal.PlainDateTime.compare(
  { year: 2000, month: 5, day: 2, hour: 12, minute: 34, second: 56, millisecond: 987, microsecond: 654, nanosecond: 321, calendar },
  { year: 2000, month: 5, day: 2, hour: 6, minute: 54, second: 32, millisecond: 123, microsecond: 456, nanosecond: 789, calendar },
);

// will be 0 if the time fields are coerced to their max values due to Infinity
assert.sameValue(result, 1, "comparison result");
