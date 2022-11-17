// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Construction and properties
features: [Temporal]
---*/

var tz = new Temporal.TimeZone("-08:00");
var epochMillis = Date.UTC(1976, 10, 18, 15, 23, 30, 123);
var epochNanos = BigInt(epochMillis) * BigInt(1000000) + BigInt(456789);

// works
var zdt = new Temporal.ZonedDateTime(epochNanos, tz);
assert(zdt instanceof Temporal.ZonedDateTime);
assert.sameValue(typeof zdt, "object");
assert.sameValue(zdt.toInstant().epochSeconds, Math.floor(Date.UTC(1976, 10, 18, 15, 23, 30, 123) / 1000), "epochSeconds");
assert.sameValue(zdt.toInstant().epochMilliseconds, Date.UTC(1976, 10, 18, 15, 23, 30, 123), "epochMilliseconds");

// Temporal.ZonedDateTime for (1976, 11, 18, 15, 23, 30, 123, 456, 789)"
  var zdt = new Temporal.ZonedDateTime(epochNanos, new Temporal.TimeZone("UTC"));
// can be constructed
assert(zdt instanceof Temporal.ZonedDateTime);
assert.sameValue(typeof zdt, "object");

assert.sameValue(zdt.year, 1976)
assert.sameValue(zdt.month, 11);
assert.sameValue(zdt.monthCode, "M11");
assert.sameValue(zdt.day, 18);
assert.sameValue(zdt.hour, 15);
assert.sameValue(zdt.minute, 23);
assert.sameValue(zdt.second, 30);
assert.sameValue(zdt.millisecond, 123);
assert.sameValue(zdt.microsecond, 456);
assert.sameValue(zdt.nanosecond, 789);
assert.sameValue(zdt.epochSeconds, 217178610);
assert.sameValue(zdt.epochMilliseconds, 217178610123);
assert.sameValue(zdt.epochMicroseconds, 217178610123456n);
assert.sameValue(zdt.epochNanoseconds, 217178610123456789n);
assert.sameValue(zdt.dayOfWeek, 4);
assert.sameValue(zdt.dayOfYear, 323);
assert.sameValue(zdt.weekOfYear, 47);
assert.sameValue(zdt.daysInWeek, 7);
assert.sameValue(zdt.daysInMonth, 30);
assert.sameValue(zdt.daysInYear, 366);
assert.sameValue(zdt.monthsInYear, 12);
assert.sameValue(zdt.inLeapYear, true);
assert.sameValue(zdt.offset, "+00:00");
assert.sameValue(zdt.offsetNanoseconds, 0);
assert.sameValue(`${ zdt }`, "1976-11-18T15:23:30.123456789+00:00[UTC]");

// Temporal.ZonedDateTime with non-UTC time zone and non-ISO calendar
// can be constructed
var fakeGregorian = {
  era() { return "ce"; },
  year(date) { return date.withCalendar("iso8601").year; },
  month(date) { return date.withCalendar("iso8601").month; },
  monthCode(date) { return date.withCalendar("iso8601").monthCode; },
  day(date) { return date.withCalendar("iso8601").day; },
  dayOfWeek(date) { return date.withCalendar("iso8601").dayOfWeek; },
  dayOfYear(date) { return date.withCalendar("iso8601").dayOfYear; },
  weekOfYear(date) { return date.withCalendar("iso8601").weekOfYear; },
  daysInWeek(date) { return date.withCalendar("iso8601").daysInWeek; },
  daysInMonth(date) { return date.withCalendar("iso8601").daysInMonth; },
  daysInYear(date) { return date.withCalendar("iso8601").daysInYear; },
  monthsInYear(date) { return date.withCalendar("iso8601").monthsInYear; },
  inLeapYear(date) { return date.withCalendar("iso8601").inLeapYear; },
  toString() { return "gregory"; },
};
var fakeVienna = {
  getOffsetNanosecondsFor() { return 3600_000_000_000; },
  toString() { return "Europe/Vienna"; },
}
var zdt = new Temporal.ZonedDateTime(epochNanos, fakeVienna, fakeGregorian);
assert(zdt instanceof Temporal.ZonedDateTime);
assert.sameValue(typeof zdt, "object");

assert.sameValue(zdt.era, "ce");
assert.sameValue(zdt.year, 1976);
assert.sameValue(zdt.month, 11);
assert.sameValue(zdt.monthCode, "M11");
assert.sameValue(zdt.day, 18);
assert.sameValue(zdt.hour, 16);
assert.sameValue(zdt.minute, 23);
assert.sameValue(zdt.second, 30);
assert.sameValue(zdt.millisecond, 123);
assert.sameValue(zdt.microsecond, 456);
assert.sameValue(zdt.nanosecond, 789);
assert.sameValue(zdt.epochSeconds, 217178610);
assert.sameValue(zdt.epochMilliseconds, 217178610123);
assert.sameValue(zdt.epochMicroseconds, 217178610123456n);
assert.sameValue(zdt.epochNanoseconds, 217178610123456789n);
assert.sameValue(zdt.dayOfWeek, 4);
assert.sameValue(zdt.dayOfYear, 323);
assert.sameValue(zdt.weekOfYear, 47);
assert.sameValue(zdt.daysInWeek, 7);
assert.sameValue(zdt.daysInMonth, 30);
assert.sameValue(zdt.daysInYear, 366);
assert.sameValue(zdt.monthsInYear, 12);
assert.sameValue(zdt.inLeapYear, true);
assert.sameValue(zdt.offset, "+01:00");
assert.sameValue(zdt.offsetNanoseconds, 3600000000000);
assert.sameValue(`${ zdt }`, "1976-11-18T16:23:30.123456789+01:00[Europe/Vienna][u-ca=gregory]");

