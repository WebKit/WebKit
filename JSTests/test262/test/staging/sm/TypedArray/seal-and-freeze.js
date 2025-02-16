// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
flags:
  - onlyStrict
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
description: |
  pending
esid: pending
---*/
"use strict";

// Seal
assert.sameValue(Object.isSealed(new Int32Array(2)), false);
assert.sameValue(Object.isSealed(new Int32Array(0)), false);

var array = new Int32Array(0);
Object.preventExtensions(array);
assert.sameValue(Object.isSealed(array), true);

// Non-empty typed arrays can never be sealed, because the elements stay configurable.
array = new Int32Array(1);
array.b = "test";
Object.preventExtensions(array);
assert.sameValue(Object.isSealed(array), false);
Object.defineProperty(array, "b", {configurable: false});
assert.sameValue(Object.isSealed(array), false);

array = new Int32Array(2);
array.b = "test";
assertThrowsInstanceOf(() => Object.seal(array), TypeError);
assert.sameValue(Object.isSealed(array), false);
assertThrowsInstanceOf(() => array.c = 15, TypeError);

// Freeze
assert.sameValue(Object.isFrozen(new Int32Array(2)), false);
assert.sameValue(Object.isFrozen(new Int32Array(0)), false);

// Empty non-extensible typed-array is trivially frozen
var array = new Int32Array(0);
Object.preventExtensions(array);
assert.sameValue(Object.isFrozen(array), true);

array = new Int32Array(0);
array.b = "test";
assert.sameValue(Object.isFrozen(array), false);
Object.preventExtensions(array);
assert.sameValue(Object.isFrozen(array), false);
Object.defineProperty(array, "b", {configurable: false, writable: false});
assert.sameValue(Object.isFrozen(array), true);

// Non-empty typed arrays can never be frozen, because the elements stay writable
array = new Int32Array(1);
assertThrowsInstanceOf(() => Object.freeze(array), TypeError);
assert.sameValue(Object.isExtensible(array), false);
assert.sameValue(Object.isFrozen(array), false);

