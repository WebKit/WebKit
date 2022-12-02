// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.with
description: The object returned from mergeFields() is copied before being passed to dateFromFields().
info: |
    sec-temporal.plaindatetime.prototype.with steps 18–19 and 23:
      18. Set _fields_ to ? CalendarMergeFields(_calendar_, _fields_, _partialDate_).
      19. Set _fields_ to ? PrepareTemporalFields(_fields_, _fieldNames_, « *"timeZone"* »).
      23. Let _dateTimeResult_ be ? InterpretTemporalDateTimeFields(_calendar_, _fields_, _options_).
    sec-temporal-interprettemporaldatetimefields step 2:
      2. Let _temporalDate_ be ? DateFromFields(_calendar_, _fields_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const expected = [
  "get day",
  "get day.valueOf",
  "call day.valueOf",
  "get hour",
  "get hour.valueOf",
  "call hour.valueOf",
  "get microsecond",
  "get microsecond.valueOf",
  "call microsecond.valueOf",
  "get millisecond",
  "get millisecond.valueOf",
  "call millisecond.valueOf",
  "get minute",
  "get minute.valueOf",
  "call minute.valueOf",
  "get month",
  "get month.valueOf",
  "call month.valueOf",
  "get monthCode",
  "get monthCode.toString",
  "call monthCode.toString",
  "get nanosecond",
  "get nanosecond.valueOf",
  "call nanosecond.valueOf",
  "get offset",
  "get offset.toString",
  "call offset.toString",
  "get second",
  "get second.valueOf",
  "call second.valueOf",
  "get timeZone",
  "get year",
  "get year.valueOf",
  "call year.valueOf",
];

const calendar = TemporalHelpers.calendarMergeFieldsGetters();
const datetime = new Temporal.ZonedDateTime(1_000_000_000_000_000_000n, "UTC", calendar);
datetime.with({ year: 2022 });

assert.compareArray(calendar.mergeFieldsReturnOperations, expected, "getters called on mergeFields return");
