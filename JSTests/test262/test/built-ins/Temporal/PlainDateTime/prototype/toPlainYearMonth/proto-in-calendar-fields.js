// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plaindatetime.prototype.toplainyearmonth
description: If a calendar's fields() method returns a field named '__proto__', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['__proto__']);
const datetime = new Temporal.PlainDateTime(2023, 5, 1, 0, 0, 0, 0, 0, 0, calendar);

assert.throws(RangeError, () => datetime.toPlainYearMonth());
