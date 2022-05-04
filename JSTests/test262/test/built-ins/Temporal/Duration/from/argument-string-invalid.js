// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.from
description: Invalid string arguments.
features: [Temporal]
---*/

const tests = [
  "P1Y1M1W1DT1H1M1.123456789123S",
  "P0.5Y",
  "P1Y0,5M",
  "P1Y1M0.5W",
  "P1Y1M1W0,5D",
  "P1Y1M1W1DT0.5H5S",
  "P1Y1M1W1DT1.5H0,5M",
  "P1Y1M1W1DT1H0.5M0.5S",
  "P",
  "PT",
  "-P",
  "-PT",
  "+P",
  "+PT",
  "P1Y1M1W1DT1H1M1.01Sjunk",
  "P-1Y1M",
  "P1Y-1M"
];

for (const input of tests) {
  assert.throws(RangeError, () => Temporal.Duration.from(input));
}
