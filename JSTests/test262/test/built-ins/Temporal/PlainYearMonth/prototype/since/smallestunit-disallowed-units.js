// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.since
description: Since throws on disallowed or invalid smallestUnit
features: [Temporal, arrow-function]
---*/

const earlier = new Temporal.PlainYearMonth(2019, 1);
const later = new Temporal.PlainYearMonth(2021, 9);

[
  'era',
  'weeks',
  'days',
  'hours',
  'minutes',
  'seconds',
  'milliseconds',
  'microseconds',
  'nanoseconds',
  'nonsense'
].forEach((smallestUnit) => {
  assert.throws(RangeError, () => later.since(earlier, { smallestUnit }),`throws on disallowed or invalid smallestUnit: ${smallestUnit}`);
});

assert.throws(RangeError, () => later.since(earlier, { largestUnit: 'months', smallestUnit: 'years' }), 'throws if smallestUnit is larger than largestUnit');
