// Copyright (C) 2019 Mathieu Hofman. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry.prototype.unregister
description: Cannot unregister a cell that has been cleaned up
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  ...
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  ...

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  ...
  3. While finalizationRegistry.[[Cells]] contains a Record cell such that cell.[[WeakRefTarget]] is ~empty~, then an implementation may perform the following steps,
    a. Choose any such cell.
    b. Remove cell from finalizationRegistry.[[Cells]].
    c. Perform ? Call(callback, undefined, << cell.[[HeldValue]] >>).
  ...

  FinalizationRegistry.prototype.unregister ( unregisterToken )

  1. Let removed be false.
  2. For each Record { [[Target]], [[Holdings]], [[UnregisterToken]] } cell that is an element of finalizationRegistry.[[Cells]], do
    a. If SameValue(cell.[[UnregisterToken]], unregisterToken) is true, then
      i. Remove cell from finalizationRegistry.[[Cells]].
      ii. Set removed to true.
  3. Return removed.
features: [FinalizationRegistry, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var value = 'target!';
var token = {};
var finalizationRegistry = new FinalizationRegistry(function() {});

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target, value, token);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  var called = 0;
  var holdings = [];
  finalizationRegistry.cleanupSome(function cb(holding) {
    called += 1;
    holdings.push(holding);
  });

  assert.sameValue(called, 1);
  assert.sameValue(holdings.length, 1);
  assert.sameValue(holdings[0], value);

  var res = finalizationRegistry.unregister(token);
  assert.sameValue(res, false, 'unregister after iterating over it in cleanup');
}).then($DONE, resolveAsyncGC);
