// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Since returns negative and positive values
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const nov94 = new Temporal.PlainYearMonth(1994, 11);
const jun13 = new Temporal.PlainYearMonth(2013, 6);
const diff = jun13.since(nov94);

TemporalHelpers.assertDurationsEqual(nov94.since(jun13), diff.negated(), 'Since is inverse of until');
