// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.fromepochseconds
description: Basic tests for Instant.fromEpochSeconds().
features: [BigInt, Temporal]
---*/

const afterEpoch = Temporal.Instant.fromEpochSeconds(217175010);
assert.sameValue(afterEpoch.epochNanoseconds, 217175010_000_000_000n, "fromEpochSeconds post epoch");

const beforeEpoch = Temporal.Instant.fromEpochSeconds(-217175010);
assert.sameValue(beforeEpoch.epochNanoseconds, -217175010_000_000_000n, "fromEpochSeconds pre epoch");
