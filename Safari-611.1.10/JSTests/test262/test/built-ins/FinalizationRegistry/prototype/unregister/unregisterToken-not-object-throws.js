// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.unregister
description: Throws a TypeError if unregisterToken is not an Object
info: |
  FinalizationRegistry.prototype.unregister ( unregisterToken )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If Type(unregisterToken) is not Object, throw a TypeError exception.
  ...
features: [FinalizationRegistry]
---*/

assert.sameValue(typeof FinalizationRegistry.prototype.unregister, 'function');

var finalizationRegistry = new FinalizationRegistry(function() {});

assert.throws(TypeError, function() {
  finalizationRegistry.unregister(undefined);
}, 'undefined');

assert.throws(TypeError, function() {
  finalizationRegistry.unregister(null);
}, 'null');

assert.throws(TypeError, function() {
  finalizationRegistry.unregister(true);
}, 'true');

assert.throws(TypeError, function() {
  finalizationRegistry.unregister(false);
}, 'false');

assert.throws(TypeError, function() {
  finalizationRegistry.unregister(1);
}, 'number');

assert.throws(TypeError, function() {
  finalizationRegistry.unregister('object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  finalizationRegistry.unregister(s);
}, 'symbol');
