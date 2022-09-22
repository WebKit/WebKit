// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-intl
description: Indian calendar
features: [Temporal]
---*/


// throws in Node 12 & 14 before 1 CE
var vulnerableToBceBug = new Date("0000-01-01T00:00Z").toLocaleDateString("en-US-u-ca-indian", { timeZone: "UTC" }) !== "10/11/-79 Saka";
if (vulnerableToBceBug) {
  assert.throws(RangeError, () => Temporal.PlainDate.from("0000-01-01").withCalendar("indian").day);
}

// handles leap days
var leapYearFirstDay = Temporal.PlainDate.from("2004-03-21[u-ca=indian]");
assert.sameValue(leapYearFirstDay.year, 2004 - 78);
assert.sameValue(leapYearFirstDay.month, 1);
assert.sameValue(leapYearFirstDay.day, 1);
var leapYearLastDay = leapYearFirstDay.with({ day: 31 });
assert.sameValue(leapYearLastDay.year, 2004 - 78);
assert.sameValue(leapYearLastDay.month, 1);
assert.sameValue(leapYearLastDay.day, 31);

// handles non-leap years
var nonLeapYearFirstDay = Temporal.PlainDate.from("2005-03-22[u-ca=indian]");
assert.sameValue(nonLeapYearFirstDay.year, 2005 - 78);
assert.sameValue(nonLeapYearFirstDay.month, 1);
assert.sameValue(nonLeapYearFirstDay.day, 1);
var leapYearLastDay = nonLeapYearFirstDay.with({ day: 31 });
assert.sameValue(leapYearLastDay.year, 2005 - 78);
assert.sameValue(leapYearLastDay.month, 1);
assert.sameValue(leapYearLastDay.day, 30);
assert.throws(RangeError, () => nonLeapYearFirstDay.with({ day: 31 }, { overflow: "reject" }));
