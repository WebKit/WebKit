// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.instant.prototype.epochseconds
description: Basic tests for epochSeconds.
features: [BigInt, Temporal]
---*/

const afterEpoch = new Temporal.Instant(217175010_123_456_789n);
assert.sameValue(afterEpoch.epochSeconds, 217175010, "epochSeconds post epoch");
assert.sameValue(typeof afterEpoch.epochSeconds, "number", "epochSeconds value is a number");

const beforeEpoch = new Temporal.Instant(-217175010_876_543_211n);
assert.sameValue(beforeEpoch.epochSeconds, -217175010, "epochSeconds pre epoch");
assert.sameValue(typeof beforeEpoch.epochSeconds, "number", "epochSeconds value is a number");
