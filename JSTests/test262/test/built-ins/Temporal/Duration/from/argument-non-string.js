// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration.from
description: Appropriate error thrown if primitive input cannot convert to a valid string
features: [Temporal]
---*/

assert.throws(RangeError, () => Temporal.Duration.from(undefined), "undefined");
assert.throws(RangeError, () => Temporal.Duration.from(null), "null");
assert.throws(RangeError, () => Temporal.Duration.from(true), "boolean");
assert.throws(TypeError, () => Temporal.Duration.from(Symbol()), "Symbol");
assert.throws(RangeError, () => Temporal.Duration.from(5), "number");
assert.throws(RangeError, () => Temporal.Duration.from(5n), "bigint");
