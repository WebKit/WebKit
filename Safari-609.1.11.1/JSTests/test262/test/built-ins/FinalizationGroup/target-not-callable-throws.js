// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  Throws a TypeError if target is not callable
info: |
  FinalizationGroup ( cleanupCallback )

  1. If NewTarget is undefined, throw a TypeError exception.
  2. If IsCallable(cleanupCallback) is false, throw a TypeError exception.
  ...
features: [FinalizationGroup]
---*/

assert.sameValue(
  typeof FinalizationGroup, 'function',
  'typeof FinalizationGroup is function'
);

assert.throws(TypeError, function() {
  new FinalizationGroup({});
}, 'ordinary object');

assert.throws(TypeError, function() {
  new FinalizationGroup(WeakRef.prototype);
}, 'WeakRef.prototype');

assert.throws(TypeError, function() {
  new FinalizationGroup(FinalizationGroup.prototype);
}, 'FinalizationGroup.prototype');

assert.throws(TypeError, function() {
  new FinalizationGroup([]);
}, 'Array');

assert.throws(TypeError, function() {
  new FinalizationGroup();
}, 'implicit undefined');

assert.throws(TypeError, function() {
  new FinalizationGroup(undefined);
}, 'explicit undefined');

assert.throws(TypeError, function() {
  new FinalizationGroup(null);
}, 'null');

assert.throws(TypeError, function() {
  new FinalizationGroup(1);
}, 'number');

assert.throws(TypeError, function() {
  new FinalizationGroup('Object');
}, 'string');

var s = Symbol();
assert.throws(TypeError, function() {
  new FinalizationGroup(s);
}, 'symbol');

assert.throws(TypeError, function() {
  new FinalizationGroup(true);
}, 'Boolean, true');

assert.throws(TypeError, function() {
  new FinalizationGroup(false);
}, 'Boolean, false');
