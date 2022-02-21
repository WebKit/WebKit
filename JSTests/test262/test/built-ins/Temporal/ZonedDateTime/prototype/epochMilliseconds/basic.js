// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.epochmilliseconds
description: Basic tests for epochMilliseconds.
features: [BigInt, Temporal]
---*/

const afterEpoch = new Temporal.ZonedDateTime(217175010_123_456_789n, "UTC");
assert.sameValue(afterEpoch.epochMilliseconds, 217175010_123, "epochMilliseconds post epoch");
assert.sameValue(typeof afterEpoch.epochMilliseconds, "number", "epochMilliseconds value is a number");

const beforeEpoch = new Temporal.ZonedDateTime(-217175010_876_543_211n, "UTC");
assert.sameValue(beforeEpoch.epochMilliseconds, -217175010_876, "epochMilliseconds pre epoch");
assert.sameValue(typeof beforeEpoch.epochMilliseconds, "number", "epochMilliseconds value is a number");
