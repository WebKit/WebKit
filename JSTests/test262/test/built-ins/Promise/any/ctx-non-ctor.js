// Copyright (C) 2019 Sergey Rubanov. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
  Promise.any invoked on a non-constructor value
esid: sec-promise.any
info: |
  ...
  2. Let promiseCapability be ? NewPromiseCapability(C).

  NewPromiseCapability ( C )

  1. If IsConstructor(C) is false, throw a TypeError exception.

features: [Promise.any, Symbol]
---*/

assert.sameValue(typeof Promise.any, 'function');

assert.throws(TypeError, function() {
  Promise.any.call(eval);
});

assert.throws(TypeError, function() {
  Promise.any.call(undefined, []);
});

assert.throws(TypeError, function() {
  Promise.any.call(null, []);
});

assert.throws(TypeError, function() {
  Promise.any.call(86, []);
});

assert.throws(TypeError, function() {
  Promise.any.call('string', []);
});

assert.throws(TypeError, function() {
  Promise.any.call(true, []);
});

assert.throws(TypeError, function() {
  Promise.any.call(Symbol(), []);
});
