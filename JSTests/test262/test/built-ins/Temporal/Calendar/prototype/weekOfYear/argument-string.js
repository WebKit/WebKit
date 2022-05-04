// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: >
  Temporal.Calendar.prototype.weekOfYear will take an ISO 8601 date string and
  return the week of year of that date.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ToISOWeekOfYear(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

// The following week numbers are taken from the table "Examples of contemporary
// dates around New Year's Day" from
// https://en.wikipedia.org/wiki/ISO_week_date#Relation_with_the_Gregorian_calendar

assert.sameValue(cal.weekOfYear("1977-01-01"), 53, "1977-01-01 is in w53");
assert.sameValue(cal.weekOfYear("1977-01-02"), 53, "1977-01-02 is in w53");
assert.sameValue(cal.weekOfYear("1977-12-31"), 52, "1977-12-31 is in w52");
assert.sameValue(cal.weekOfYear("1978-01-01"), 52, "1978-01-01 is in w52");
assert.sameValue(cal.weekOfYear("1978-01-02"), 1, "1978-01-02 is in w01");
assert.sameValue(cal.weekOfYear("1978-12-31"), 52, "1978-12-31 is in w52");
assert.sameValue(cal.weekOfYear("1979-01-01"), 1, "1979-01-01 is in w01");
assert.sameValue(cal.weekOfYear("1979-12-30"), 52, "1979-12-30 is in w52");
assert.sameValue(cal.weekOfYear("1979-12-31"), 1, "1979-12-31 is in w01");
assert.sameValue(cal.weekOfYear("1980-01-01"), 1, "1980-01-01 is in w01");
assert.sameValue(cal.weekOfYear("1980-12-28"), 52, "1980-12-28 is in w52");
assert.sameValue(cal.weekOfYear("1980-12-29"), 1, "1980-12-29 is in w01");
assert.sameValue(cal.weekOfYear("1980-12-30"), 1, "1980-12-30 is in w01");
assert.sameValue(cal.weekOfYear("1980-12-31"), 1, "1980-12-31 is in w01");
assert.sameValue(cal.weekOfYear("1981-01-01"), 1, "1981-01-01 is in w01");
assert.sameValue(cal.weekOfYear("1981-12-31"), 53, "1981-12-31 is in w53");
assert.sameValue(cal.weekOfYear("1982-01-01"), 53, "1982-01-01 is in w53");
assert.sameValue(cal.weekOfYear("1982-01-02"), 53, "1982-01-02 is in w53");
assert.sameValue(cal.weekOfYear("1982-01-03"), 53, "1982-01-03 is in w53");
