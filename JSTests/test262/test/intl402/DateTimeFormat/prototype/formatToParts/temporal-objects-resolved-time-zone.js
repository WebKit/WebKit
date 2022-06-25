// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.DateTimeFormat.prototype.formatToParts
description: A time zone in resolvedOptions with a large offset still produces the correct string
locale: [en]
includes: [deepEqual.js]
features: [Temporal]
---*/

const formatter = new Intl.DateTimeFormat("en", { timeZone: "Pacific/Apia" });

const date = new Temporal.PlainDate(2021, 8, 4);
const dateResult = formatter.formatToParts(date);
assert.deepEqual(dateResult, [
  { type: "month", value: "8" },
  { type: "literal", value: "/" },
  { type: "day", value: "4" },
  { type: "literal", value: "/" },
  { type: "year", value: "2021" },
], "plain date");

const datetime1 = new Temporal.PlainDateTime(2021, 8, 4, 0, 30, 45, 123, 456, 789);
const datetimeResult1 = formatter.formatToParts(datetime1);
assert.deepEqual(datetimeResult1, [
  { type: "month", value: "8" },
  { type: "literal", value: "/" },
  { type: "day", value: "4" },
  { type: "literal", value: "/" },
  { type: "year", value: "2021" },
  { type: "literal", value: ", " },
  { type: "hour", value: "12" },
  { type: "literal", value: ":" },
  { type: "minute", value: "30" },
  { type: "literal", value: ":" },
  { type: "second", value: "45" },
  { type: "literal", value: " " },
  { type: "dayPeriod", value: "AM" },
], "plain datetime close to beginning of day");
const datetime2 = new Temporal.PlainDateTime(2021, 8, 4, 23, 30, 45, 123, 456, 789);
const datetimeResult2 = formatter.formatToParts(datetime2);
assert.deepEqual(datetimeResult2, [
  { type: "month", value: "8" },
  { type: "literal", value: "/" },
  { type: "day", value: "4" },
  { type: "literal", value: "/" },
  { type: "year", value: "2021" },
  { type: "literal", value: ", " },
  { type: "hour", value: "11" },
  { type: "literal", value: ":" },
  { type: "minute", value: "30" },
  { type: "literal", value: ":" },
  { type: "second", value: "45" },
  { type: "literal", value: " " },
  { type: "dayPeriod", value: "PM" },
], "plain datetime close to end of day");

const monthDay = new Temporal.PlainMonthDay(8, 4, "gregory");
const monthDayResult = formatter.formatToParts(monthDay);
assert.deepEqual(monthDayResult, [
  { type: "month", value: "8" },
  { type: "literal", value: "/" },
  { type: "day", value: "4" },
], "plain month-day");

const time1 = new Temporal.PlainTime(0, 30, 45, 123, 456, 789);
const timeResult1 = formatter.formatToParts(time1);
assert.deepEqual(timeResult1, [
  { type: "hour", value: "12" },
  { type: "literal", value: ":" },
  { type: "minute", value: "30" },
  { type: "literal", value: ":" },
  { type: "second", value: "45" },
  { type: "literal", value: " " },
  { type: "dayPeriod", value: "AM" },
], "plain time close to beginning of day");
const time2 = new Temporal.PlainTime(23, 30, 45, 123, 456, 789);
const timeResult2 = formatter.formatToParts(time2);
assert.deepEqual(timeResult2, [
  { type: "hour", value: "11" },
  { type: "literal", value: ":" },
  { type: "minute", value: "30" },
  { type: "literal", value: ":" },
  { type: "second", value: "45" },
  { type: "literal", value: " " },
  { type: "dayPeriod", value: "PM" },
], "plain time close to end of day");

const month = new Temporal.PlainYearMonth(2021, 8, "gregory");
const monthResult = formatter.formatToParts(month);
assert.deepEqual(monthResult, [
  { type: "month", value: "8" },
  { type: "literal", value: "/" },
  { type: "year", value: "2021" },
], "plain year-month");
