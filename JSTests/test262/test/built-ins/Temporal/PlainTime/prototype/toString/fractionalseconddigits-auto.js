// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: auto value for fractionalSecondDigits option
features: [Temporal]
---*/

const tests = [
  ["15:23", "15:23:00"],
  ["15:23:30", "15:23:30"],
  ["15:23:30.1234", "15:23:30.1234"],
];

for (const [input, expected] of tests) {
  const plainTime = Temporal.PlainTime.from(input);
  assert.sameValue(plainTime.toString({ fractionalSecondDigits: "auto" }), expected);
}
