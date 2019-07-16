// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.register
description: Throws a TypeError if target is not an Object
info: |
  FinalizationGroup.prototype.register ( target , holdings [, unregisterToken ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If Type(target) is not Object, throw a TypeError exception.
  4. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  5. If Type(unregisterToken) is not Object,
    a. If unregisterToken is not undefined, throw a TypeError exception.
    b. Set unregisterToken to empty.
  ...
features: [FinalizationGroup]
---*/

assert.sameValue(typeof FinalizationGroup.prototype.register, 'function');

var fg = new FinalizationGroup(function() {});

assert.throws(TypeError, function() {
  fg.register(undefined);
}, 'undefined');

assert.throws(TypeError, function() {
  fg.register(null);
}, 'null');

assert.throws(TypeError, function() {
  fg.register(true);
}, 'true');

assert.throws(TypeError, function() {
  fg.register(false);
}, 'false');

assert.throws(TypeError, function() {
  fg.register(1);
}, 'number');

assert.throws(TypeError, function() {
  fg.register('object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  fg.register(s);
}, 'symbol');
