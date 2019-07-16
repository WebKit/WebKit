// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Throws a TypeError if this is not an Object
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  ...
features: [FinalizationGroup]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.cleanupSome, 'function');

var cleanupSome = FinalizationGroup.prototype.cleanupSome;
var cb = function() {};

assert.throws(TypeError, function() {
  cleanupSome.call(undefined, cb);
}, 'undefined');

assert.throws(TypeError, function() {
  cleanupSome.call(null, cb);
}, 'null');

assert.throws(TypeError, function() {
  cleanupSome.call(true, cb);
}, 'true');

assert.throws(TypeError, function() {
  cleanupSome.call(false, cb);
}, 'false');

assert.throws(TypeError, function() {
  cleanupSome.call(1, cb);
}, 'number');

assert.throws(TypeError, function() {
  cleanupSome.call('object', cb);
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  cleanupSome.call(s, cb);
}, 'symbol');
