// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tostring
description: auto value for fractionalSecondDigits option
features: [BigInt, Temporal]
---*/

const zeroSeconds = new Temporal.Instant(0n);
const wholeSeconds = new Temporal.Instant(30_000_000_000n);
const subSeconds = new Temporal.Instant(30_123_400_000n);

const tests = [
  [zeroSeconds, "1970-01-01T00:00:00Z"],
  [wholeSeconds, "1970-01-01T00:00:30Z"],
  [subSeconds, "1970-01-01T00:00:30.1234Z"],
];

for (const [instant, expected] of tests) {
  assert.sameValue(instant.toString(), expected, "default is to emit seconds and drop trailing zeroes");
  assert.sameValue(instant.toString({ fractionalSecondDigits: "auto" }), expected, "auto is the default");
}
