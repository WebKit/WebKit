// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  FinalizationRegistryCleanupIteratorPrototype.next() throws if this is not Object
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  ...
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  ...

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  ...
  3. Let iterator be ! CreateFinalizationRegistryCleanupIterator(finalizationRegistry).
  ...
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  %FinalizationRegistryCleanupIteratorPrototype%.next ( )

  1. Let iterator be the this value.
  2. If Type(iterator) is not Object, throw a TypeError exception.
features: [FinalizationRegistry, host-gc-required, Symbol]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var FinalizationRegistryCleanupIteratorPrototype;
var called = 0;
var finalizationRegistry = new FinalizationRegistry(function() {});

function callback(iterator) {
  called += 1;
  FinalizationRegistryCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(callback);

  assert.sameValue(called, 1, 'cleanup successful');

  assert.sameValue(typeof FinalizationRegistryCleanupIteratorPrototype.next, 'function');

  var next = FinalizationRegistryCleanupIteratorPrototype.next;

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
