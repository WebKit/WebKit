// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: Passing a primitive other than string to add() throws
features: [Symbol, Temporal]
---*/

const instance = new Temporal.PlainDate(2000, 5, 2);
assert.throws(TypeError, () => instance.add(undefined), "undefined");
assert.throws(TypeError, () => instance.add(null), "null");
assert.throws(TypeError, () => instance.add(true), "boolean");
assert.throws(RangeError, () => instance.add(""), "empty string");
assert.throws(TypeError, () => instance.add(Symbol()), "Symbol");
assert.throws(TypeError, () => instance.add(7), "number");
assert.throws(TypeError, () => instance.add(7n), "bigint");
assert.throws(TypeError, () => instance.add([]), "array");
assert.throws(TypeError, () => instance.add(() => {}), "function");
