// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.compare
description: The time fields are read from the object before being passed to dateFromFields().
info: |
    sec-temporal.duration.compare step 4:
      4. Let _relativeTo_ be ? ToRelativeTemporalObject(_options_).
    sec-temporal-torelativetemporalobject step 4.g:
      g. Let _result_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields steps 1–2:
      1. Let _timeResult_ be ? ToTemporalTimeRecord(_fields_).
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarMakeInvalidGettersTime();
const duration1 = new Temporal.Duration(1);
const duration2 = new Temporal.Duration(0, 12);
Temporal.Duration.compare(duration1, duration2, { relativeTo: { year: 2000, month: 1, day: 1, calendar } });
