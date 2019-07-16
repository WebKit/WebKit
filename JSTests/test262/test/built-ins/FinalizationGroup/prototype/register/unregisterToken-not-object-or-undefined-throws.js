// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.register
description: Throws a TypeError if unregisterToken is not an Object or undefined
info: |
  FinalizationGroup.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If Type(target) is not Object, throw a TypeError exception.
  4. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  5. If Type(unregisterToken) is not Object,
    a. If unregisterToken is not undefined, throw a TypeError exception.
  ...
features: [FinalizationGroup]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.register, 'function');

var fg = new FinalizationGroup(function() {});
var target = {};

assert.throws(TypeError, function() {
  fg.register(target, undefined, null);
}, 'null');

assert.throws(TypeError, function() {
  fg.register(target, undefined, true);
}, 'true');

assert.throws(TypeError, function() {
  fg.register(target, undefined, false);
}, 'false');

assert.throws(TypeError, function() {
  fg.register(target, undefined, 1);
}, 'number');

assert.throws(TypeError, function() {
  fg.register(target, undefined, 'object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  fg.register(target, undefined, s);
}, 'symbol');
