// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  The cleanupSome() method throws if cleanup is currently in progress. 
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  ...
  4. If finalizationRegistry.[[IsFinalizationRegistryCleanupJobActive]] is true, 
     throw a TypeError exception.

features: [FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called = 0;
var endOfCall = 0;
var finalizationRegistry = new FinalizationRegistry(function() {});

function callback(iterator) {
  called += 1;

  if (called === 1) {
    // Atempt to re-enter the callback.
    var nestedCallbackRan = false;
    assert.throws(TypeError, () => {
      finalizationRegistry.cleanupSome(() => { nestedCallbackRan = true });
    });
    assert.sameValue(nestedCallbackRan, false);
  }

  endOfCall += 1;
}

function emptyCells() {
  var o1 = {};
  finalizationRegistry.register(o1, 'holdings 1');

  var prom = asyncGC(o1);
  o1 = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(callback);

  assert.sameValue(called, 1, 'callback was called');
  assert.sameValue(endOfCall, 1, 'callback finished');
}).then($DONE, resolveAsyncGC);
