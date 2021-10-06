// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.equals
description: Appropriate error thrown when argument cannot be converted to a valid string
features: [Symbol, Temporal]
---*/

const instance = Temporal.Instant.fromEpochSeconds(0);

assert.throws(RangeError, () => instance.equals(undefined), "undefined");
assert.throws(RangeError, () => instance.equals(null), "null");
assert.throws(RangeError, () => instance.equals(true), "true");
assert.throws(RangeError, () => instance.equals(""), "empty string");
assert.throws(TypeError, () => instance.equals(Symbol()), "symbol");
assert.throws(RangeError, () => instance.equals(1), "1");
assert.throws(RangeError, () => instance.equals({}), "plain object");
assert.throws(RangeError, () => instance.equals(Temporal.Instant), "Temporal.Instant");
assert.throws(TypeError, () => instance.equals(Temporal.Instant.prototype), "Temporal.Instant.prototype");
