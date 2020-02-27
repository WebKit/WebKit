// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Return abrupt completion from CleanupFinalizationRegistry
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  4. If callback is undefined, set callback to finalizationRegistry.[[CleanupCallback]].
  5. Set finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  7. Set finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] to false.
  8. If result is an abrupt completion, return result.
features: [FinalizationRegistry, arrow-function, async-functions, async-iteration, class, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called = 0;
var iterator = false;

function poisoned(iter) {
  called += 1;
  iterator = iter;
  iter.next(); // Won't throw
  throw new Test262Error();
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
  assert.throws(Test262Error, function() {
    finalizationRegistry.cleanupSome(poisoned);
  });

  assert.sameValue(called, 1);

  assert.sameValue(typeof iterator, 'object');
  assert.sameValue(typeof iterator.next, 'function');
  assert.throws(TypeError, function() {
    iterator.next();
  }, 'iterator.next throws a TypeError if IsFinalizationRegistryCleanupJobActive is false');
}).then($DONE, resolveAsyncGC);
