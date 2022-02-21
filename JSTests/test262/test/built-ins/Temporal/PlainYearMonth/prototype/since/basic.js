// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Basic calls to PYM.since.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const feb20 = new Temporal.PlainYearMonth(2020, 2);
const feb21 = new Temporal.PlainYearMonth(2021, 2);
const oneYear = Temporal.Duration.from('P1Y');
const twelveMonths = Temporal.Duration.from('P12M');

TemporalHelpers.assertDurationsEqual(feb21.since(feb20), oneYear, 'Returns year by default');
TemporalHelpers.assertDurationsEqual(feb21.since(feb20,  { largestUnit: 'auto' }), oneYear, 'Returns year by default');

TemporalHelpers.assertDurationsEqual(feb21.since(feb20, { largestUnit: 'years' }), oneYear, 'Returns year explicitly');
TemporalHelpers.assertDurationsEqual(feb21.since(feb20, { largestUnit: 'months' }), twelveMonths, 'Returns months when requested.')
