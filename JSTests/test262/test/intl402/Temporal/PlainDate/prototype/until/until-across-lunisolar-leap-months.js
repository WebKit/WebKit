// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: dateUntil works as expected after a leap month in a lunisolar calendar
esid: sec-temporal.plaindate.prototype.until
features: [Temporal]
---*/

// 2001 is a leap year in the Chinese calendar with a M04L leap month.
// Therefore, month: 6 is M05 in 2001 but M06 in 2000 which is not a leap year.

const year2000 = new Temporal.PlainDate(2000, 3, 1).withCalendar("chinese").year;
const year2001 = new Temporal.PlainDate(2001, 3, 1).withCalendar("chinese").year;

const one = Temporal.PlainDate.from({ year: year2000, month: 6, day: 1, calendar: 'chinese' });
const two = Temporal.PlainDate.from({ year: year2001, month: 6, day: 1, calendar: 'chinese' });

assert.sameValue(one.inLeapYear, false, "year 2000 is not a leap year");
assert.sameValue(one.monthCode, "M06", "sixth month in year 2000 has month code M06");

assert.sameValue(two.inLeapYear, true, "year 2001 is a leap year");
assert.sameValue(two.monthCode, "M05", "sixth month in year 2001 has month code M05");

const expected = { years: 'P12M', months: 'P12M', weeks: 'P50W4D', days: 'P354D' };

Object.entries(expected).forEach(([largestUnit, expectedResult]) => {
  const actualResult = one.until(two, { largestUnit });
  assert.sameValue(actualResult.toString(), expectedResult);
});
