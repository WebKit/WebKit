// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group-target
description: >
  cleanupCallback has only one optional chance to be called for a GC that cleans up
  a registered target.
info: |
  FinalizationGroup ( cleanupCallback )

  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  ...
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.

  Execution

  At any time, if an object obj is not live, an ECMAScript implementation may perform the following steps atomically:

  1. For each WeakRef ref such that ref.[[Target]] is obj,
    a. Set ref.[[Target]] to empty.
  2. For each FinalizationGroup fg such that fg.[[Cells]] contains cell, such that cell.[[Target]] is obj,
    a. Set cell.[[Target]] to empty.
    b. Optionally, perform ! HostCleanupFinalizationGroup(fg).
features: [FinalizationGroup, async-functions, host-gc-required]
flags: [async, non-deterministic]
includes: [async-gc.js]
---*/

var cleanupCallback = 0;
var called = 0;

// both this cb and the fg callback are not exhausting the iterator to prevent
// the target cell from being removed from the finalizationGroup.[[Cells]].
// More info at %FinalizationGroupCleanupIteratorPrototype%.next ( )
function cb() {
  called += 1;
}

var fg = new FinalizationGroup(function() {
  cleanupCallback += 1;
});

function emptyCells() {
  var target = {};
  fg.register(target, 'a');

  var prom = asyncGC(target);
  target = null;

  return prom;
}

// Let's add some async ticks, as the cleanupCallback is not meant to interrupt
// synchronous operations.
async function fn() {
  await Promise.resolve(1);

  fg.cleanupSome(cb);
  // This asserts the registered object was emptied in the previous GC.
  assert.sameValue(called, 1, 'cleanupSome callback for the first time');

  // At this point, we can't assert if cleanupCallback was called, because it's
  // optional. Although, we can finally assert it's not gonna be called anymore
  // for the other executions of the Garbage Collector.
  // The chance of having it called only happens right after the
  // cell.[[Target]] is set to empty.
  assert(cleanupCallback >= 0, 'cleanupCallback might be 0');
  assert(cleanupCallback <= 1, 'cleanupCallback might be 1');

  // Restoring the cleanupCallback variable to 0 will help us asserting the fg
  // callback is not called again.
  cleanupCallback = 0;

  await $262.gc();
  await Promise.resolve(2); // tick

  fg.cleanupSome(cb);

  // This cb is called again because fg still holds an emptied cell, but it's
  // not yet removed from the 
  assert.sameValue(called, 2, 'cleanupSome callback for the second time');
  assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #1');

  await $262.gc();
  await Promise.resolve(3); // tick

  fg.cleanupSome(cb);

  assert.sameValue(called, 3, 'cleanupSome callback for the third time');
  assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #2');

  await $262.gc();
}

emptyCells()
  .then(async function() {
    await fn();// tick
    await Promise.resolve(4); // tick

    assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #3');

    fg.cleanupSome(cb);

    assert.sameValue(called, 4, 'cleanupSome callback for the fourth time');
    assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #4');
    
    await $262.gc();

    // Now we are exhausting the iterator, so cleanupSome callback will also not be called.
    fg.cleanupSome(iterator => {
      var exhausted = [...iterator];
      assert.sameValue(exhausted.length, 1);
      assert.sameValue(exhausted[0], 'a');
      called += 1;
    });

    assert.sameValue(called, 5, 'cleanupSome callback for the fifth time');
    assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #4');

    await $262.gc();

    await Promise.resolve(5); // tick
    await await Promise.resolve(6); // more ticks
    await await await Promise.resolve(7); // even more ticks

    fg.cleanupSome(cb);

    assert.sameValue(called, 5, 'cleanupSome callback is not called anymore, no empty cells');
    assert.sameValue(cleanupCallback, 0, 'cleanupCallback is not called again #5');
  })
  .then($DONE, resolveAsyncGC);
