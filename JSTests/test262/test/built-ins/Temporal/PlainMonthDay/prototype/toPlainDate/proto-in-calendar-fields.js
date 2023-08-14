// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: If a calendar's fields() method returns a field named '__proto__', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['__proto__']);
const md = new Temporal.PlainMonthDay(5, 1, calendar);

assert.throws(RangeError, () => md.toPlainDate({year: 2023}));
