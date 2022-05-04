// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.prototype.add
description: Verify that undefined options are handled correctly.
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const duration1 = new Temporal.Duration(1);
const duration2 = new Temporal.Duration(0, 12);
const duration3 = new Temporal.Duration(0, 0, 0, 1);
const duration4 = new Temporal.Duration(0, 0, 0, 0, 24);

assert.throws(RangeError, () => duration1.add(duration2), "no options with years");
TemporalHelpers.assertDuration(duration3.add(duration4),
  0, 0, 0, /* days = */ 2, 0, 0, 0, 0, 0, 0,
  "no options with days");

const optionValues = [
  [undefined, "undefined"],
  [{}, "plain object"],
  [() => {}, "lambda"],
];
for (const [options, description] of optionValues) {
  assert.throws(RangeError, () => duration1.add(duration2, options),
    `options ${description} with years`);
  TemporalHelpers.assertDuration(duration3.add(duration4, options),
    0, 0, 0, /* days = */ 2, 0, 0, 0, 0, 0, 0,
    `options ${description} with days`);
}
