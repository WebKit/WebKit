// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.instant.prototype.epochseconds
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const epochSeconds = Object.getOwnPropertyDescriptor(Temporal.Instant.prototype, "epochSeconds").get;

assert.sameValue(typeof epochSeconds, "function");

assert.throws(TypeError, () => epochSeconds.call(undefined), "undefined");
assert.throws(TypeError, () => epochSeconds.call(null), "null");
assert.throws(TypeError, () => epochSeconds.call(true), "true");
assert.throws(TypeError, () => epochSeconds.call(""), "empty string");
assert.throws(TypeError, () => epochSeconds.call(Symbol()), "symbol");
assert.throws(TypeError, () => epochSeconds.call(1), "1");
assert.throws(TypeError, () => epochSeconds.call({}), "plain object");
assert.throws(TypeError, () => epochSeconds.call(Temporal.Instant), "Temporal.Instant");
assert.throws(TypeError, () => epochSeconds.call(Temporal.Instant.prototype), "Temporal.Instant.prototype");
