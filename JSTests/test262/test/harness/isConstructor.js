// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Including isConstructor.js will expose one function:

      isConstructor

includes: [isConstructor.js]
features: [Reflect.construct]
---*/

assert.sameValue(typeof isConstructor, "function");

assert.sameValue(isConstructor(), false);
assert.sameValue(isConstructor(undefined), false);
assert.sameValue(isConstructor(null), false);
assert.sameValue(isConstructor(123), false);
assert.sameValue(isConstructor(true), false);
assert.sameValue(isConstructor(false), false);
assert.sameValue(isConstructor("string"), false);

assert.sameValue(isConstructor({}), false);
assert.sameValue(isConstructor([]), false);

assert.sameValue(isConstructor(function(){}), true);
assert.sameValue(isConstructor(function*(){}), false);
assert.sameValue(isConstructor(() => {}), false);

assert.sameValue(isConstructor(Array), true);
assert.sameValue(isConstructor(Array.prototype.map), false);
