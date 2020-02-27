// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  Throws a TypeError if [[IsFinalizationRegistryCleanupJobActive]] is false
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
  5. Set finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  7. Set finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] to false.
  ...

  %FinalizationRegistryCleanupIteratorPrototype%.next ( )

  1. Let iterator be the this value.
  2. If Type(iterator) is not Object, throw a TypeError exception.
  3. If iterator does not have a [[FinalizationRegistry]] internal slot, throw a TypeError exception.
features: [FinalizationRegistry, WeakRef, host-gc-required, Symbol]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var iter;
var FinalizationRegistryCleanupIteratorPrototype;
var called = 0;
var finalizationRegistry = new FinalizationRegistry(function() {});

function callback(iterator) {
  called += 1;
  iter = iterator;
  FinalizationRegistryCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target, 'target');

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(callback);

  // Make sure everything is set
  assert.sameValue(called, 1, 'cleanup successful');
  assert.sameValue(typeof iter, 'object');
  assert.sameValue(Object.getPrototypeOf(iter), FinalizationRegistryCleanupIteratorPrototype);

  // To the actual assertion
  assert.throws(TypeError, function() {
    iter.next();
  }, 'Iter should fail if not called during the cleanupSome call');
}).then($DONE, resolveAsyncGC);
