// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Throws a TypeError if callback is not callable
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

var fg = new FinalizationGroup(function() {});

assert.throws(TypeError, function() {
  fg.cleanupSome(null);
}, 'null');

assert.throws(TypeError, function() {
  fg.cleanupSome(true);
}, 'true');

assert.throws(TypeError, function() {
  fg.cleanupSome(false);
}, 'false');

assert.throws(TypeError, function() {
  fg.cleanupSome(1);
}, 'number');

assert.throws(TypeError, function() {
  fg.cleanupSome('object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  fg.cleanupSome(s);
}, 'symbol');

assert.throws(TypeError, function() {
  fg.cleanupSome({});
}, 'object');

assert.throws(TypeError, function() {
  fg.cleanupSome(FinalizationGroup.prototype);
}, 'FinalizationGroup.prototype');
