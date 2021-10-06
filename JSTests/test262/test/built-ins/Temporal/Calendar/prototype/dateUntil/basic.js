// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Basic tests for dateUntil().
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const iso = Temporal.Calendar.from("iso8601");
const date1 = Temporal.PlainDate.from("1999-09-03");
const date2 = Temporal.PlainDate.from("2000-01-01");

TemporalHelpers.assertDuration(
  iso.dateUntil(date1, date2, {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "two PlainDates");

TemporalHelpers.assertDuration(
  iso.dateUntil(Temporal.PlainDateTime.from("1999-09-03T08:15:30"), date2, {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "first argument: PlainDateTime");

TemporalHelpers.assertDuration(
  iso.dateUntil({ year: 1999, month: 9, day: 3 }, date2, {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "first argument: property bag");

TemporalHelpers.assertDuration(
  iso.dateUntil("1999-09-03", date2, {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "first argument: string");

assert.throws(TypeError, () => iso.dateUntil({ month: 11 }, date2, {}), "first argument: missing property");

TemporalHelpers.assertDuration(
  iso.dateUntil(date1, Temporal.PlainDateTime.from("2000-01-01T08:15:30"), {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "second argument: PlainDateTime");

TemporalHelpers.assertDuration(
  iso.dateUntil(date1, { year: 2000, month: 1, day: 1 }, {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "second argument: property bag");

TemporalHelpers.assertDuration(
  iso.dateUntil(date1, "2000-01-01", {}),
  0, 0, 0, 120, 0, 0, 0, 0, 0, 0, "second argument: string");

assert.throws(TypeError, () => iso.dateUntil(date1, { month: 11 }, {}), "second argument: missing property");
