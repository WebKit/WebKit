// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: >
  Temporal.Calendar.prototype.weekOfYear will take Temporal.PlainDateTime object
  and return the week of year of that date.
info: |
  4. Let temporalDate be ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ToISOWeekOfYear(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

// The following week numbers are taken from the table "Examples of contemporary
// dates around New Year's Day" from
// https://en.wikipedia.org/wiki/ISO_week_date#Relation_with_the_Gregorian_calendar

let dt = new Temporal.PlainDateTime(1977, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1977-01-01 is in w53");

dt = new Temporal.PlainDateTime(1977, 1, 2, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1977-01-02 is in w53");

dt = new Temporal.PlainDateTime(1977, 12, 31, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 52, "1977-12-31 is in w52");

dt = new Temporal.PlainDateTime(1978, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 52, "1978-01-01 is in w52");

dt = new Temporal.PlainDateTime(1978, 1, 2, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1978-01-02 is in w01");

dt = new Temporal.PlainDateTime(1978, 12, 31, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 52, "1978-12-31 is in w52");

dt = new Temporal.PlainDateTime(1979, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1979-01-01 is in w01");

dt = new Temporal.PlainDateTime(1979, 12, 30, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 52, "1979-12-30 is in w52");

dt = new Temporal.PlainDateTime(1979, 12, 31, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1979-12-31 is in w01");

dt = new Temporal.PlainDateTime(1980, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1980-01-01 is in w01");

dt = new Temporal.PlainDateTime(1980, 12, 28, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 52, "1980-12-28 is in w52");

dt = new Temporal.PlainDateTime(1980, 12, 29, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1980-12-29 is in w01");

dt = new Temporal.PlainDateTime(1980, 12, 30, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1980-12-30 is in w01");

dt = new Temporal.PlainDateTime(1980, 12, 31, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1980-12-31 is in w01");

dt = new Temporal.PlainDateTime(1981, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 1, "1981-01-01 is in w01");

dt = new Temporal.PlainDateTime(1981, 12, 31, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1981-12-31 is in w53");

dt = new Temporal.PlainDateTime(1982, 1, 1, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1982-01-01 is in w53");

dt = new Temporal.PlainDateTime(1982, 1, 2, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1982-01-02 is in w53");

dt = new Temporal.PlainDateTime(1982, 1, 3, 9, 8);
assert.sameValue(cal.weekOfYear(dt), 53, "1982-01-03 is in w53");
