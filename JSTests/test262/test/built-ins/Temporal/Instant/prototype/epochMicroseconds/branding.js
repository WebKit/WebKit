// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.instant.prototype.epochmicroseconds
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const epochMicroseconds = Object.getOwnPropertyDescriptor(Temporal.Instant.prototype, "epochMicroseconds").get;

assert.sameValue(typeof epochMicroseconds, "function");

assert.throws(TypeError, () => epochMicroseconds.call(undefined), "undefined");
assert.throws(TypeError, () => epochMicroseconds.call(null), "null");
assert.throws(TypeError, () => epochMicroseconds.call(true), "true");
assert.throws(TypeError, () => epochMicroseconds.call(""), "empty string");
assert.throws(TypeError, () => epochMicroseconds.call(Symbol()), "symbol");
assert.throws(TypeError, () => epochMicroseconds.call(1), "1");
assert.throws(TypeError, () => epochMicroseconds.call({}), "plain object");
assert.throws(TypeError, () => epochMicroseconds.call(Temporal.Instant), "Temporal.Instant");
assert.throws(TypeError, () => epochMicroseconds.call(Temporal.Instant.prototype), "Temporal.Instant.prototype");
