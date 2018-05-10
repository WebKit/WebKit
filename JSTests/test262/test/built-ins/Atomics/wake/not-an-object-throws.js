// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-atomics.wake
description: >
  Throws a TypeError if typedArray arg is not an Object
info: |
  Atomics.wake( typedArray, index, count )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
    2. if Type(typedArray) is not Object, throw a TypeError exception
features: [Atomics, Symbol]
---*/

var poisoned = {
  valueOf: function() {
    throw new Test262Error("should not evaluate this code");
  }
};

assert.throws(TypeError, function() {
  Atomics.wake(null, poisoned, poisoned);
}, 'null');

assert.throws(TypeError, function() {
  Atomics.wake(undefined, poisoned, poisoned);
}, 'undefined');

assert.throws(TypeError, function() {
  Atomics.wake(true, poisoned, poisoned);
}, 'true');

assert.throws(TypeError, function() {
  Atomics.wake(false, poisoned, poisoned);
}, 'false');

assert.throws(TypeError, function() {
  Atomics.wake('***string***', poisoned, poisoned);
}, 'String');

assert.throws(TypeError, function() {
  Atomics.wake(Number.NEGATIVE_INFINITY, poisoned, poisoned);
}, '-Infinity');

assert.throws(TypeError, function() {
  Atomics.wake(Symbol('***symbol***'), poisoned, poisoned);
}, 'Symbol');
