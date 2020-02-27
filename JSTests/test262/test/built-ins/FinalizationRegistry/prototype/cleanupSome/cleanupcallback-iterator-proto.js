// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  The cleanup callback function is called with a FinalizationRegistryCleanupIterator
info: |
  The %FinalizationRegistryCleanupIteratorPrototype% Object

  - has properties that are inherited by all FinalizationRegistry Cleanup Iterator Objects.
  - is an ordinary object.
  - has a [[Prototype]] internal slot whose value is the intrinsic object %IteratorPrototype%.
  ...

  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  ...
  2. If CheckForEmptyCells(finalizationRegistry) is false, return.
  3. Let iterator be ! CreateFinalizationRegistryCleanupIterator(finalizationRegistry).
  4. If callback is undefined, set callback to finalizationRegistry.[[CleanupCallback]].
  5. Set finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  CheckForEmptyCells ( finalizationRegistry )

  ...
  2. For each cell in finalizationRegistry.[[Cells]], do
    a. If cell.[[Target]] is empty, then
      i. Return true.
  3. Return false.

  CreateFinalizationRegistryCleanupIterator ( finalizationRegistry )

  ...
  4. Let prototype be finalizationRegistry.[[Realm]].[[Intrinsics]].[[%FinalizationRegistryCleanupIteratorPrototype%]].
  5. Let iterator be ObjectCreate(prototype, « [[FinalizationRegistry]] »).
  6. Set iterator.[[FinalizationRegistry]] to finalizationRegistry.
  7. Return iterator.
features: [FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var IteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
var FinalizationRegistryCleanupIteratorPrototype;
var called = 0;

function callback(iterator) {
  called += 1;

  FinalizationRegistryCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

var finalizationRegistry = new FinalizationRegistry(function() {});

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(callback);
  assert.sameValue(called, 1);

  var proto = Object.getPrototypeOf(FinalizationRegistryCleanupIteratorPrototype);
  assert.sameValue(
    proto, IteratorPrototype,
    '[[Prototype]] internal slot whose value is the intrinsic object %IteratorPrototype%'
  );
}).then($DONE, resolveAsyncGC);
