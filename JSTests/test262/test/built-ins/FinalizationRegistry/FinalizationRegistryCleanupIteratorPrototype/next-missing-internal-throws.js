// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  If iterator does not have a [[FinalizationRegistry]] internal slot, throw a TypeError exception.
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
  3. If iterator does not have a [[FinalizationRegistry]] internal slot, throw a TypeError exception.
features: [FinalizationRegistry, WeakRef, host-gc-required, Symbol]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var FinalizationRegistryCleanupIteratorPrototype;
var called = 0;
var endOfCall = 0;
var finalizationRegistry = new FinalizationRegistry(function() {});

function callback(iterator) {
  called += 1;
  // Only the iterator itself will have a [[FinalizationRegistry]] internal
  FinalizationRegistryCleanupIteratorPrototype = Object.getPrototypeOf(iterator);

  // It's sensitive the assertions remain inside this function in order to secure
  // [[IsFinalizationRegistryCleanupJobActive]] is true
  assert.sameValue(typeof FinalizationRegistryCleanupIteratorPrototype.next, 'function');

  var next = FinalizationRegistryCleanupIteratorPrototype.next;

  assert.throws(TypeError, function() {
    next.call({});
  }, '{}');

  assert.throws(TypeError, function() {
    next.call(FinalizationRegistry);
  }, 'FinalizationRegistry');

  assert.throws(TypeError, function() {
    next.call(FinalizationRegistryCleanupIteratorPrototype);
  }, 'FinalizationRegistryCleanupIteratorPrototype');

  assert.throws(TypeError, function() {
    next.call(finalizationRegistry);
  }, 'FinalizationRegistry instance');

  var wr = new WeakRef({});
  assert.throws(TypeError, function() {
    next.call(wr);
  }, 'WeakRef instance');

  // Abrupt completions are not directly returned.
  endOfCall += 1;
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
  assert.sameValue(endOfCall, 1, 'Abrupt completions are not directly returned.');
}).then($DONE, resolveAsyncGC);
