// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: RangeError thrown when adding negative duration to last representable month.
features: [Temporal]
---*/

const lastMonth = new Temporal.PlainYearMonth(275760, 9);

// See https://tc39.es/proposal-temporal/#sec-temporal-adddurationtoyearmonth
// (step 10d)
assert.throws(RangeError, () => lastMonth.add({seconds: -1}));
assert.throws(RangeError, () => lastMonth.add({minutes: -1}));
assert.throws(RangeError, () => lastMonth.add({hours: -1}));
assert.throws(RangeError, () => lastMonth.add({days: -1}));
assert.throws(RangeError, () => lastMonth.add({weeks: -1}));
assert.throws(RangeError, () => lastMonth.add({months: -1}));
assert.throws(RangeError, () => lastMonth.add({years: -1}));

