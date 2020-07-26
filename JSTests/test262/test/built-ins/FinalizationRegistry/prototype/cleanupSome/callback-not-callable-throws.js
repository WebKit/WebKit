// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Throws a TypeError if callback is not callable
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  ...
features: [cleanupSome, FinalizationRegistry]
---*/

assert.sameValue(typeof FinalizationRegistry.prototype.cleanupSome, 'function');

var finalizationRegistry = new FinalizationRegistry(function() {});

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(null);
}, 'null');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(true);
}, 'true');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(false);
}, 'false');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(1);
}, 'number');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome('object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(s);
}, 'symbol');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome({});
}, 'object');

assert.throws(TypeError, function() {
  finalizationRegistry.cleanupSome(FinalizationRegistry.prototype);
}, 'FinalizationRegistry.prototype');
