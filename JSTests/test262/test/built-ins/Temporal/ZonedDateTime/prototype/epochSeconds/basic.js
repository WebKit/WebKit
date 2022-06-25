// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.epochseconds
description: Basic tests for epochSeconds.
features: [BigInt, Temporal]
---*/

const afterEpoch = new Temporal.ZonedDateTime(217175010_123_456_789n, "UTC");
assert.sameValue(afterEpoch.epochSeconds, 217175010, "epochSeconds post epoch");
assert.sameValue(typeof afterEpoch.epochSeconds, "number", "epochSeconds value is a number");

const beforeEpoch = new Temporal.ZonedDateTime(-217175010_876_543_211n, "UTC");
assert.sameValue(beforeEpoch.epochSeconds, -217175010, "epochSeconds pre epoch");
assert.sameValue(typeof beforeEpoch.epochSeconds, "number", "epochSeconds value is a number");
