// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plaindate.from
description: With year, monthCode and day and need constrain
info: |
  1. Let calendar be the this value.
  2. Perform ? RequireInternalSlot(calendar, [[InitializedTemporalCalendar]]).
  3. Assert: calendar.[[Identifier]] is "iso8601".
  4. If Type(fields) is not Object, throw a TypeError exception.
  5. Set options to ? GetOptionsObject(options).
  6. Let result be ? ISODateFromFields(fields, options).
  7. Return ? CreateTemporalDate(result.[[Year]], result.[[Month]], result.[[Day]], calendar).
features: [Temporal]
includes: [temporalHelpers.js]
---*/

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M01", day: 133}),
    2021, 1, "M01", 31,
    "year/monthCode/day with day need to be constrained in Jan");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M02", day: 133}),
    2021, 2, "M02", 28,
    "year/monthCode/day with day need to be constrained in Feb");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M03", day: 133}),
    2021, 3, "M03", 31,
    "year/monthCode/day with day need to be constrained in March");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M04", day: 133}),
    2021, 4, "M04", 30,
    "year/monthCode/day with day need to be constrained in April");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M05", day: 133}),
    2021, 5, "M05", 31,
    "year/monthCode/day with day need to be constrained in May");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M06", day: 133}),
    2021, 6, "M06", 30,
    "year/monthCode/day with day need to be constrained in Jun");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M07", day: 133}),
    2021, 7, "M07", 31,
    "year/monthCode/day with day need to be constrained in July");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M08", day: 133}),
    2021, 8, "M08", 31,
    "year/monthCode/day with day need to be constrained in Aug");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M09", day: 133}),
    2021, 9, "M09", 30,
    "year/monthCode/day with day need to be constrained in Sept.");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M10", day: 133}),
    2021, 10, "M10", 31,
    "year/monthCode/day with day need to be constrained in Oct.");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M11", day: 133}),
    2021, 11, "M11", 30,
    "year/monthCode/day with day need to be constrained in Nov.");

TemporalHelpers.assertPlainDate(
    Temporal.PlainDate.from({year: 2021, monthCode: "M12", day: 133}),
    2021, 12, "M12", 31,
    "year/monthCode/day with day need to be constrained in Dec.");
