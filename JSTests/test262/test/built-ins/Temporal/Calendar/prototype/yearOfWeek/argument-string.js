// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: >
  Temporal.Calendar.prototype.yearOfWeek will take an ISO 8601 date string and
  return the ISO week calendar year of that date.
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

// The following week calendar years are taken from the table "Examples of
// contemporary dates around New Year's Day" from
// https://en.wikipedia.org/wiki/ISO_week_date#Relation_with_the_Gregorian_calendar

assert.sameValue(cal.yearOfWeek("1977-01-01"), 1976, "1977-01-01 is in yearOfWeek 1976");
assert.sameValue(cal.yearOfWeek("1977-01-02"), 1976, "1977-01-02 is in yearOfWeek 1976");
assert.sameValue(cal.yearOfWeek("1977-12-31"), 1977, "1977-12-31 is in yearOfWeek 1977");
assert.sameValue(cal.yearOfWeek("1978-01-01"), 1977, "1978-01-01 is in yearOfWeek 1977");
assert.sameValue(cal.yearOfWeek("1978-01-02"), 1978, "1978-01-02 is in yearOfWeek 1978");
assert.sameValue(cal.yearOfWeek("1978-12-31"), 1978, "1978-12-31 is in yearOfWeek 1978");
assert.sameValue(cal.yearOfWeek("1979-01-01"), 1979, "1979-01-01 is in yearOfWeek 1979");
assert.sameValue(cal.yearOfWeek("1979-12-30"), 1979, "1979-12-30 is in yearOfWeek 1979");
assert.sameValue(cal.yearOfWeek("1979-12-31"), 1980, "1979-12-31 is in yearOfWeek 1980");
assert.sameValue(cal.yearOfWeek("1980-01-01"), 1980, "1980-01-01 is in yearOfWeek 1980");
assert.sameValue(cal.yearOfWeek("1980-12-28"), 1980, "1980-12-28 is in yearOfWeek 1980");
assert.sameValue(cal.yearOfWeek("1980-12-29"), 1981, "1980-12-29 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1980-12-30"), 1981, "1980-12-30 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1980-12-31"), 1981, "1980-12-31 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1981-01-01"), 1981, "1981-01-01 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1981-12-31"), 1981, "1981-12-31 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1982-01-01"), 1981, "1982-01-01 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1982-01-02"), 1981, "1982-01-02 is in yearOfWeek 1981");
assert.sameValue(cal.yearOfWeek("1982-01-03"), 1981, "1982-01-03 is in yearOfWeek 1981");
