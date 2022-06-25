// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.withcalendar
description: Basic tests for withCalendar().
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const plainDate = Temporal.PlainDate.from("1976-11-18");
const calendar = Temporal.Calendar.from("iso8601");

const objectResult = plainDate.withCalendar(calendar);
assert.notSameValue(objectResult, plainDate, "object: new object");
TemporalHelpers.assertPlainDate(objectResult, 1976, 11, "M11", 18, "object");
assert.sameValue(objectResult.calendar, calendar, "object: calendar");

const stringResult = plainDate.withCalendar("iso8601");
assert.notSameValue(stringResult, plainDate, "string: new object");
TemporalHelpers.assertPlainDate(stringResult, 1976, 11, "M11", 18, "string");
assert.sameValue(stringResult.calendar.id, "iso8601", "string: calendar");

const originalCalendar = plainDate.calendar;
const sameResult = plainDate.withCalendar(originalCalendar);
assert.notSameValue(sameResult, plainDate, "original: new object");
TemporalHelpers.assertPlainDate(sameResult, 1976, 11, "M11", 18, "original");
assert.sameValue(sameResult.calendar, originalCalendar, "original: calendar");
