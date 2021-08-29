// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.zoneddatetime.prototype.epochseconds
description: Throw a TypeError if the receiver is invalid
features: [Symbol, Temporal]
---*/

const epochSeconds = Object.getOwnPropertyDescriptor(Temporal.ZonedDateTime.prototype, "epochSeconds").get;

assert.sameValue(typeof epochSeconds, "function");

assert.throws(TypeError, () => epochSeconds.call(undefined), "undefined");
assert.throws(TypeError, () => epochSeconds.call(null), "null");
assert.throws(TypeError, () => epochSeconds.call(true), "true");
assert.throws(TypeError, () => epochSeconds.call(""), "empty string");
assert.throws(TypeError, () => epochSeconds.call(Symbol()), "symbol");
assert.throws(TypeError, () => epochSeconds.call(1), "1");
assert.throws(TypeError, () => epochSeconds.call({}), "plain object");
assert.throws(TypeError, () => epochSeconds.call(Temporal.ZonedDateTime), "Temporal.ZonedDateTime");
assert.throws(TypeError, () => epochSeconds.call(Temporal.ZonedDateTime.prototype), "Temporal.ZonedDateTime.prototype");
