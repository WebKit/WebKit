// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-instant-objects
description: Min/max range
features: [Temporal]
---*/


// constructing from ns
var limit = 8640000000000000000000n;
assert.throws(RangeError, () => new Temporal.Instant(-limit - 1n));
assert.throws(RangeError, () => new Temporal.Instant(limit + 1n));
assert.sameValue(`${ new Temporal.Instant(-limit) }`, "-271821-04-20T00:00:00Z");
assert.sameValue(`${ new Temporal.Instant(limit) }`, "+275760-09-13T00:00:00Z");

// constructing from ms
var limit = 8640000000000000;
assert.throws(RangeError, () => Temporal.Instant.fromEpochMilliseconds(-limit - 1));
assert.throws(RangeError, () => Temporal.Instant.fromEpochMilliseconds(limit + 1));
assert.sameValue(`${ Temporal.Instant.fromEpochMilliseconds(-limit) }`, "-271821-04-20T00:00:00Z");
assert.sameValue(`${ Temporal.Instant.fromEpochMilliseconds(limit) }`, "+275760-09-13T00:00:00Z");
