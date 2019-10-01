// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  FinalizationGroupCleanupIteratorPrototype.next() throws if this is not Object
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  ...
  5. Perform ! CleanupFinalizationGroup(finalizationGroup, callback).
  ...

  CleanupFinalizationGroup ( finalizationGroup [ , callback ] )

  ...
  3. Let iterator be ! CreateFinalizationGroupCleanupIterator(finalizationGroup).
  ...
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  %FinalizationGroupCleanupIteratorPrototype%.next ( )

  1. Let iterator be the this value.
  2. If Type(iterator) is not Object, throw a TypeError exception.
features: [FinalizationGroup, host-gc-required, Symbol]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var FinalizationGroupCleanupIteratorPrototype;
var called = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;
  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

function emptyCells() {
  var target = {};
  fg.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(callback);

  assert.sameValue(called, 1, 'cleanup successful');

  assert.sameValue(typeof FinalizationGroupCleanupIteratorPrototype.next, 'function');

  var next = FinalizationGroupCleanupIteratorPrototype.next;

  assert.throws(TypeError, function() {
    next.call(undefined);
  }, 'undefined');

  assert.throws(TypeError, function() {
    next.call(null);
  }, 'null');

  assert.throws(TypeError, function() {
    next.call(true);
  }, 'true');

  assert.throws(TypeError, function() {
    next.call(false);
  }, 'false');

  assert.throws(TypeError, function() {
    next.call(1);
  }, '1');

  assert.throws(TypeError, function() {
    next.call('string');
  }, 'string');

  var symbol = Symbol();
  assert.throws(TypeError, function() {
    next.call(symbol);
  }, 'symbol');
}).then($DONE, resolveAsyncGC);
