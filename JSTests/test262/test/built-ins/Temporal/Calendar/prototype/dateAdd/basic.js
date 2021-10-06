// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Basic tests for dateAdd().
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const date = Temporal.PlainDate.from("1994-11-06");
const positiveDuration = Temporal.Duration.from({ months: 1, weeks: 1 });
const negativeDuration = Temporal.Duration.from({ months: -1, weeks: -1 });

TemporalHelpers.assertPlainDate(
  iso.dateAdd(Temporal.PlainDateTime.from("1994-11-06T08:15:30"), positiveDuration, {}),
  1994, 12, "M12", 13, "date: PlainDateTime");

TemporalHelpers.assertPlainDate(
  iso.dateAdd({ year: 1994, month: 11, day: 6 }, positiveDuration, {}),
  1994, 12, "M12", 13, "date: property bag");

TemporalHelpers.assertPlainDate(
  iso.dateAdd("1994-11-06", positiveDuration, {}),
  1994, 12, "M12", 13, "date: string");

assert.throws(TypeError, () => iso.dateAdd({ month: 11 }, positiveDuration, {}), "date: missing property");

TemporalHelpers.assertPlainDate(
  iso.dateAdd(date, { months: 1, weeks: 1 }, {}),
  1994, 12, "M12", 13, "duration: property bag");

TemporalHelpers.assertPlainDate(
  iso.dateAdd(date, "P1M1W", {}),
  1994, 12, "M12", 13, "duration: string");

assert.throws(TypeError, () => iso.dateAdd(date, { month: 1 }, {}), "duration: missing property");

TemporalHelpers.assertPlainDate(
  iso.dateAdd(Temporal.PlainDateTime.from("1994-11-06T08:15:30"), negativeDuration, {}),
  1994, 9, "M09", 29, "date: PlainDateTime, negative duration");

TemporalHelpers.assertPlainDate(
  iso.dateAdd({ year: 1994, month: 11, day: 6 }, negativeDuration, {}),
  1994, 9, "M09", 29, "date: property bag, negative duration");

TemporalHelpers.assertPlainDate(
  iso.dateAdd("1994-11-06", negativeDuration, {}),
  1994, 9, "M09", 29, "date: string, negative duration");
