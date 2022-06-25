// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinmonth
description: >
  Temporal.Calendar.prototype.daysInMonth will take ISO8601 string
  and return the number of days in that month.
info: |
  4. If Type(temporalDateLike) is not Object or temporalDateLike does not have
    an [[InitializedTemporalDate]] or [[InitializedTemporalYearMonth]] internal
    slots, then
    a. Set temporalDateLike to ? ToTemporalDate(temporalDateLike).
  5. Return 𝔽(! ISODaysInMonth(temporalDateLike.[[ISOYear]], temporalDateLike.[[ISOMonth]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.sameValue(cal.daysInMonth("2019-01-18"), 31, 'cal.daysInMonth("2019-01-18") must return 31');
// leap year
assert.sameValue(cal.daysInMonth("2020-02-18"), 29, 'cal.daysInMonth("2020-02-18") must return 29');
// non leap
assert.sameValue(cal.daysInMonth("2019-02-18"), 28, 'cal.daysInMonth("2019-02-18") must return 28');
assert.sameValue(cal.daysInMonth("2019-03-18"), 31, 'cal.daysInMonth("2019-03-18") must return 31');
assert.sameValue(cal.daysInMonth("2019-04-18"), 30, 'cal.daysInMonth("2019-04-18") must return 30');
assert.sameValue(cal.daysInMonth("2019-05-18"), 31, 'cal.daysInMonth("2019-05-18") must return 31');
assert.sameValue(cal.daysInMonth("2019-06-18"), 30, 'cal.daysInMonth("2019-06-18") must return 30');
assert.sameValue(cal.daysInMonth("2019-07-18"), 31, 'cal.daysInMonth("2019-07-18") must return 31');
assert.sameValue(cal.daysInMonth("2019-08-18"), 31, 'cal.daysInMonth("2019-08-18") must return 31');
assert.sameValue(cal.daysInMonth("2019-09-18"), 30, 'cal.daysInMonth("2019-09-18") must return 30');
assert.sameValue(cal.daysInMonth("2019-10-18"), 31, 'cal.daysInMonth("2019-10-18") must return 31');
assert.sameValue(cal.daysInMonth("2019-11-18"), 30, 'cal.daysInMonth("2019-11-18") must return 30');
assert.sameValue(cal.daysInMonth("2019-12-18"), 31, 'cal.daysInMonth("2019-12-18") must return 31');
