// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Cleanup might be prevented with a reference usage;
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.
features: [FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var holdingsList = [];
function cb(holding) {
  holdingsList.push(holding);
};
var finalizationRegistry = new FinalizationRegistry(function() {});

var referenced = {};

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target, 'target!');
  finalizationRegistry.register(referenced, 'referenced');

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(cb);

  assert.sameValue(holdingsList.length, 1);
  assert.sameValue(holdingsList[0], 'target!');

  assert.sameValue(typeof referenced, 'object', 'referenced preserved');
}).then($DONE, resolveAsyncGC);
