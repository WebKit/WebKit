// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Throws a TypeError if this does not have a [[Cells]] internal slot
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  ...
features: [FinalizationGroup, WeakRef]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.cleanupSome, 'function');

var cleanupSome = FinalizationGroup.prototype.cleanupSome;
var cb = function() {};

assert.throws(TypeError, function() {
  cleanupSome.call({ ['[[Cells]]']: {} }, cb);
}, 'Ordinary object without [[Cells]]');

assert.throws(TypeError, function() {
  cleanupSome.call(WeakRef.prototype, cb);
}, 'WeakRef.prototype does not have a [[Cells]] internal slot');

assert.throws(TypeError, function() {
  cleanupSome.call(WeakRef, cb);
}, 'WeakRef does not have a [[Cells]] internal slot');

var wr = new WeakRef({});
assert.throws(TypeError, function() {
  cleanupSome.call(wr, cb);
}, 'WeakRef instance');

var wm = new WeakMap();
assert.throws(TypeError, function() {
  cleanupSome.call(wm, cb);
}, 'WeakMap instance');

var ws = new WeakSet();
assert.throws(TypeError, function() {
  cleanupSome.call(ws, cb);
}, 'WeakSet instance');
