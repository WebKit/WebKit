// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Since rounding increments work as expected
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2019, 1);
const later = new Temporal.PlainYearMonth(2021, 9);

const laterSinceYear = later.since(earlier, { smallestUnit: 'years', roundingIncrement: 4, roundingMode: 'halfExpand' });
TemporalHelpers.assertDurationsEqual(laterSinceYear, Temporal.Duration.from('P4Y'), 'rounds to an increment of years');

const laterSinceMixed = later.since(earlier, { smallestUnit: 'months', roundingIncrement: 5 });
TemporalHelpers.assertDurationsEqual(laterSinceMixed, Temporal.Duration.from('P2Y5M'), 'rounds to an increment of months mixed with years');

const laterSinceMonth = later.since(earlier, { largestUnit: 'months', smallestUnit: 'months', roundingIncrement: 10 });
TemporalHelpers.assertDurationsEqual(laterSinceMonth, Temporal.Duration.from('P30M'), 'rounds to an increment of pure months');
