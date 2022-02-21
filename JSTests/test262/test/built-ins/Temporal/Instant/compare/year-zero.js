// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.compare
description: Negative zero, as an extended year, fails
features: [Temporal]
---*/

const instance = new Temporal.Instant(0n);
const bad = '-000000-03-30T00:45Z';

assert.throws(RangeError,
  () => Temporal.Instant.compare(bad, instance),
  "minus zero is invalid extended year (first argument)");
assert.throws(RangeError,
  () => Temporal.Instant.compare(instance, bad),
  "minus zero is invalid extended year (second argument)"
);
