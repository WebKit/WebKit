// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: If a calendar's fields() method returns a field named 'constructor', PrepareTemporalFields should throw a RangeError.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarWithExtraFields(['constructor']);
const ym = new Temporal.PlainYearMonth(2023, 5, calendar);

assert.throws(RangeError, () => ym.subtract({days: 123}));
