// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  The cleanup callback function is called with a FinalizationGroupCleanupIterator
info: |
  The %FinalizationGroupCleanupIteratorPrototype% Object

  - has properties that are inherited by all FinalizationGroup Cleanup Iterator Objects.
  - is an ordinary object.
  - has a [[Prototype]] internal slot whose value is the intrinsic object %IteratorPrototype%.
  ...

  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ! CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.

  CleanupFinalizationGroup ( finalizationGroup [ , callback ] )

  ...
  2. If CheckForEmptyCells(finalizationGroup) is false, return.
  3. Let iterator be ! CreateFinalizationGroupCleanupIterator(finalizationGroup).
  4. If callback is undefined, set callback to finalizationGroup.[[CleanupCallback]].
  5. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  CheckForEmptyCells ( finalizationGroup )

  ...
  2. For each cell in finalizationGroup.[[Cells]], do
    a. If cell.[[Target]] is empty, then
      i. Return true.
  3. Return false.

  CreateFinalizationGroupCleanupIterator ( finalizationGroup )

  ...
  4. Let prototype be finalizationGroup.[[Realm]].[[Intrinsics]].[[%FinalizationGroupCleanupIteratorPrototype%]].
  5. Let iterator be ObjectCreate(prototype, « [[FinalizationGroup]] »).
  6. Set iterator.[[FinalizationGroup]] to finalizationGroup.
  7. Return iterator.
features: [FinalizationGroup, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var IteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
var FinalizationGroupCleanupIteratorPrototype;
var called = 0;

function callback(iterator) {
  called += 1;

  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

var fg = new FinalizationGroup(function() {});

function emptyCells() {
  var target = {};
  fg.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(callback);
  assert.sameValue(called, 1);

  var proto = Object.getPrototypeOf(FinalizationGroupCleanupIteratorPrototype);
  assert.sameValue(
    proto, IteratorPrototype,
    '[[Prototype]] internal slot whose value is the intrinsic object %IteratorPrototype%'
  );
}).then($DONE, resolveAsyncGC);
