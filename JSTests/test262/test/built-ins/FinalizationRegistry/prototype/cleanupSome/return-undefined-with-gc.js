// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Return undefined regardless the result of CleanupFinalizationRegistry
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.
features: [cleanupSome, FinalizationRegistry, arrow-function, async-functions, async-iteration, class, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called;
var fn = function() {
  called += 1;
  return 39;
};
var cb = function() {
  called += 1;
  return 42;
};
var finalizationRegistry = new FinalizationRegistry(fn);

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  called = 0;
  assert.sameValue(finalizationRegistry.cleanupSome(cb), undefined, 'regular callback');
  assert.sameValue(called, 1);
}).then(emptyCells).then(function() {
  called = 0;
  assert.sameValue(finalizationRegistry.cleanupSome(fn), undefined, 'regular callback, same FG cleanup function');
  assert.sameValue(called, 1);
}).then(emptyCells).then(function() {
  called = 0;
  assert.sameValue(finalizationRegistry.cleanupSome(), undefined, 'undefined (implicit) callback, defer to FB callback');
  assert.sameValue(called, 1);
}).then(emptyCells).then(function() {
  called = 0;
  assert.sameValue(finalizationRegistry.cleanupSome(undefined), undefined, 'undefined (explicit) callback, defer to FB callback');
  assert.sameValue(called, 1);
}).then(emptyCells).then(function() {
  assert.sameValue(finalizationRegistry.cleanupSome(() => 1), undefined, 'arrow function');
}).then(emptyCells).then(function() {
  assert.sameValue(finalizationRegistry.cleanupSome(async function() {}), undefined, 'async function');
}).then(emptyCells).then(function() {
  assert.sameValue(finalizationRegistry.cleanupSome(function *() {}), undefined, 'generator');
}).then(emptyCells).then(function() {
  assert.sameValue(finalizationRegistry.cleanupSome(async function *() {}), undefined, 'async generator');
}).then($DONE, resolveAsyncGC);
