// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.toplaindate
description: Throws a TypeError if the argument is not an Object, before any other observable actions
includes: [compareArray.js, temporalHelpers.js]
features: [BigInt, Symbol, Temporal]
---*/

[null, undefined, true, 3.1416, "a string", Symbol("symbol"), 7n].forEach((primitive) => {
  const calendar = TemporalHelpers.calendarThrowEverything();
  const plainYearMonth = new Temporal.PlainYearMonth(2000, 5, calendar);
  assert.throws(TypeError, () => plainYearMonth.toPlainDate(primitive));
});
