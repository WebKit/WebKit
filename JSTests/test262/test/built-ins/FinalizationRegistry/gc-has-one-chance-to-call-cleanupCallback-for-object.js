// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-registry-target
description: >
  cleanupCallback has only one optional chance to be called for a GC that cleans
  up a registered Object target.
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ _callback_ ] )
  3. If _callback_ is present and IsCallable(_callback_) is *false*, throw a
    *TypeError* exception.
  4. Perform ? CleanupFinalizationRegistry(_finalizationRegistry_, _callback_).
  5. Return *undefined*.

  Execution

  At any time, if a set of objects and/or symbols _S_ is not live, an ECMAScript
  implementation may perform the following steps atomically:

  1. For each element _value_ of _S_, do
    ...
    b. For each FinalizationRegistry _fg_ such that _fg_.[[Cells]] contains a
      Record _cell_ such that _cell_.[[WeakRefTarget]] is _value_,
      i. Set _cell_.[[WeakRefTarget]] to ~empty~.
      ii. Optionally, perform HostEnqueueFinalizationRegistryCleanupJob(_fg_).
features: [FinalizationRegistry.prototype.cleanupSome, FinalizationRegistry, async-functions, host-gc-required]
flags: [async, non-deterministic]
includes: [async-gc.js, compareArray.js]
---*/

let cleanupCallback = 0;
let holdings = [];
function cb(holding) {
  holdings.push(holding);
}

let finalizationRegistry = new FinalizationRegistry(function() {
  cleanupCallback += 1;
});

function emptyCells() {
  let target = {};
  finalizationRegistry.register(target, 'a');

  let prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(async function() {
  await Promise.resolve(1);

  finalizationRegistry.cleanupSome(cb);

  // cleanupSome will be invoked if there are empty cells left. If the
  // cleanupCallback already ran, then cb won't be called.
  let expectedCalled = cleanupCallback === 1 ? 0 : 1;
  // This asserts the registered object was emptied in the previous GC.
  assert.sameValue(holdings.length, expectedCalled, 'cleanupSome callback for the first time');

  // At this point, we can't assert if cleanupCallback was called, because it's
  // optional. Although, we can finally assert it's not gonna be called anymore
  // for the other executions of the Garbage Collector.
  // The chance of having it called only happens right after the
  // cell.[[WeakRefTarget]] is set to empty.
  assert(cleanupCallback >= 0, 'cleanupCallback might be 0');
  assert(cleanupCallback <= 1, 'cleanupCallback might be 1');

  // Restoring the cleanupCallback variable to 0 will help us asserting the
  // finalizationRegistry callback is not called again.
  cleanupCallback = 0;

  await $262.gc();
  await Promise.resolve(2); // tick

  finalizationRegistry.cleanupSome(cb);

  assert.sameValue(holdings.length, expectedCalled, 'cleanupSome callback is not called anymore, no empty cells');
  assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #1');

  await $262.gc();
  await Promise.resolve(3); // tick

  finalizationRegistry.cleanupSome(cb);

  assert.sameValue(holdings.length, expectedCalled, 'cleanupSome callback is not called again #2');
  assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #2');

  if (holdings.length) {
    assert.compareArray(holdings, ['a']);
  }

  await $262.gc();
}).then($DONE, resolveAsyncGC);
