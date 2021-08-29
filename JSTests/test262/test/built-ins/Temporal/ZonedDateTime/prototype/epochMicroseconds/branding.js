// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.epochmicroseconds
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const epochMicroseconds = Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime.prototype, "epochMicroseconds").get;

assert.sameValue(typeof epochMicroseconds, "function");

assert.throws(TypeError, () => epochMicroseconds.call(undefined), "undefined");
assert.throws(TypeError, () => epochMicroseconds.call(null), "null");
assert.throws(TypeError, () => epochMicroseconds.call(true), "true");
assert.throws(TypeError, () => epochMicroseconds.call(""), "empty string");
assert.throws(TypeError, () => epochMicroseconds.call(Symbol()), "symbol");
assert.throws(TypeError, () => epochMicroseconds.call(1), "1");
assert.throws(TypeError, () => epochMicroseconds.call({}), "plain object");
assert.throws(TypeError, () => epochMicroseconds.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => epochMicroseconds.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
