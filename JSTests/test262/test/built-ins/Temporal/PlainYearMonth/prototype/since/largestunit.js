// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Specify behavior of PlainYearMonth.since when largest specified unit is years or months.
includes: [temporalHelpers.js]
features: [Temporal]
---*/
const nov2020 = new Temporal.PlainYearMonth(2020, 11);
const dec2021 = new Temporal.PlainYearMonth(2021, 12);
TemporalHelpers.assertDuration(dec2021.since(nov2020), 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'does not include higher units than necessary (largest unit unspecified)');
TemporalHelpers.assertDuration(dec2021.since(nov2020, { largestUnit: 'years' }), 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'does not include higher units than necessary (largest unit is years)');
TemporalHelpers.assertDuration(dec2021.since(nov2020, { largestUnit: 'months' }), 0, 13, 0, 0, 0, 0, 0, 0, 0, 0,  'does not include higher units than necessary (largest unit is months)');
