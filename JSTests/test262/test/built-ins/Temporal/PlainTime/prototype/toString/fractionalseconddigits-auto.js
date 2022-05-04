// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: auto value for fractionalSecondDigits option
features: [Temporal]
---*/

const zeroSeconds = new Temporal.PlainTime(15, 23);
const wholeSeconds = new Temporal.PlainTime(15, 23, 30);
const subSeconds = new Temporal.PlainTime(15, 23, 30, 123, 400);

const tests = [
  [zeroSeconds, "15:23:00"],
  [wholeSeconds, "15:23:30"],
  [subSeconds, "15:23:30.1234"],
];

for (const [time, expected] of tests) {
  assert.sameValue(time.toString(), expected, "default is to emit seconds and drop trailing zeroes");
  assert.sameValue(time.toString({ fractionalSecondDigits: "auto" }), expected, "auto is the default");
}
