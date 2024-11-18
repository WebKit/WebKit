// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: RangeError thrown when subtracting duration from last representable month.
features: [Temporal]
---*/

const lastMonth = new Temporal.PlainYearMonth(275760, 9);

// See https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
// (step 10d)
assert.throws(RangeError, () => lastMonth.subtract({seconds: 1}));
assert.throws(RangeError, () => lastMonth.subtract({minutes: 1}));
assert.throws(RangeError, () => lastMonth.subtract({hours: 1}));
assert.throws(RangeError, () => lastMonth.subtract({days: 1}));
assert.throws(RangeError, () => lastMonth.subtract({weeks: 1}));
assert.throws(RangeError, () => lastMonth.subtract({months: 1}));
assert.throws(RangeError, () => lastMonth.subtract({years: 1}));

