// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.fromepochmicroseconds
description: Basic tests for Instant.fromEpochMicroseconds().
features: [BigInt, Temporal]
---*/

const afterEpoch = Temporal.Instant.fromEpochMicroseconds(217175010_123_456n);
assert.sameValue(afterEpoch.epochNanoseconds, 217175010_123_456_000n, "fromEpochMicroseconds post epoch");

const beforeEpoch = Temporal.Instant.fromEpochMicroseconds(-217175010_876_543n);
assert.sameValue(beforeEpoch.epochNanoseconds, -217175010_876_543_000n, "fromEpochMicroseconds pre epoch");
