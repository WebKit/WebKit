// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.instant.prototype.epochmicroseconds
description: Basic tests for epochMicroseconds.
features: [BigInt, Temporal]
---*/

const afterEpoch = new Temporal.Instant(217175010_123_456_789n);
assert.sameValue(afterEpoch.epochMicroseconds, 217175010_123_456n, "epochMicroseconds post epoch");
assert.sameValue(typeof afterEpoch.epochMicroseconds, "bigint", "epochMicroseconds value is a bigint");

const beforeEpoch = new Temporal.Instant(-217175010_876_543_211n);
assert.sameValue(beforeEpoch.epochMicroseconds, -217175010_876_543n, "epochMicroseconds pre epoch");
assert.sameValue(typeof beforeEpoch.epochMicroseconds, "bigint", "epochMicroseconds value is a bigint");
