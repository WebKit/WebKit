// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.register
description: Throws a TypeError if this does not have a [[Cells]] internal slot
info: |
  FinalizationGroup.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If Type(target) is not Object, throw a TypeError exception.
  4. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  ...
features: [FinalizationGroup, WeakRef]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.register, 'function');

var register = FinalizationGroup.prototype.register;
var target = {};

assert.throws(TypeError, function() {
  register.call({ ['[[Cells]]']: {} }, target);
}, 'Ordinary object without [[Cells]]');

assert.throws(TypeError, function() {
  register.call(WeakRef.prototype, target);
}, 'WeakRef.prototype does not have a [[Cells]] internal slot');

assert.throws(TypeError, function() {
  register.call(WeakRef, target);
}, 'WeakRef does not have a [[Cells]] internal slot');

var wr = new WeakRef({});
assert.throws(TypeError, function() {
  register.call(wr, target);
}, 'WeakRef instance');

var wm = new WeakMap();
assert.throws(TypeError, function() {
  register.call(wm, target);
}, 'WeakMap instance');

var ws = new WeakSet();
assert.throws(TypeError, function() {
  register.call(ws, target);
}, 'WeakSet instance');
