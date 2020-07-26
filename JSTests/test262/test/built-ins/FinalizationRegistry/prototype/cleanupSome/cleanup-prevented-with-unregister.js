// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.cleanupSome
description: Cleanup might be prevented with an unregister usage
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  2. If Type(finalizationRegistry) is not Object, throw a TypeError exception.
  3. If finalizationRegistry does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationRegistry(finalizationRegistry, callback).
  6. Return undefined.

  FinalizationRegistry.prototype.unregister ( unregisterToken )

  1. Let removed be false.
  2. For each Record { [[Target]], [[Holdings]], [[UnregisterToken]] } cell that is an element of finalizationRegistry.[[Cells]], do
    a. If SameValue(cell.[[UnregisterToken]], unregisterToken) is true, then
      i. Remove cell from finalizationRegistry.[[Cells]].
      ii. Set removed to true.
  3. Return removed.
features: [cleanupSome, FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var token = {};
var finalizationRegistry = new FinalizationRegistry(function() {});

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target, 'target!', token);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  var called = 0;

  var res = finalizationRegistry.unregister(token);
  assert.sameValue(res, true, 'unregister target before iterating over it in cleanup');

  finalizationRegistry.cleanupSome(function cb(holding) {
    called += 1;
  });
  
  assert.sameValue(called, 0, 'callback was not called');
}).then($DONE, resolveAsyncGC);
