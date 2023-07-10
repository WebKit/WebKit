// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-zoneddatetime-objects
description: Temporal.ZonedDateTime.prototype.withTimeZone()
features: [Temporal]
---*/

// keeps instant and calendar the same
var fakeGregorian = {
  dateAdd() {},
  dateFromFields() {},
  dateUntil() {},
  day() {},
  dayOfWeek() {},
  dayOfYear() {},
  daysInMonth() {},
  daysInWeek() {},
  daysInYear() {},
  fields() {},
  id: "gregory",
  inLeapYear() {},
  mergeFields() {},
  month() {},
  monthCode() {},
  monthDayFromFields() {},
  monthsInYear() {},
  weekOfYear() {},
  year() {},
  yearMonthFromFields() {},
  yearOfWeek() {},
};
var zdt = Temporal.ZonedDateTime.from("2019-11-18T15:23:30.123456789+01:00[+01:00]").withCalendar(fakeGregorian);
var zdt2 = zdt.withTimeZone("-08:00");
assert.sameValue(zdt.epochNanoseconds, zdt2.epochNanoseconds);
assert.sameValue(zdt2.getCalendar(), fakeGregorian);
assert.sameValue(zdt2.timeZoneId, "-08:00");
assert.notSameValue(`${ zdt.toPlainDateTime() }`, `${ zdt2.toPlainDateTime() }`);
