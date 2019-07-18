// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Return abrupt completion from CleanupFinalizationGroup (using CleanupCallback)
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.

  CleanupFinalizationGroup ( finalizationGroup [ , callback ] )

  4. If callback is undefined, set callback to finalizationGroup.[[CleanupCallback]].
  5. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  7. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to false.
  8. If result is an abrupt completion, return result.
features: [FinalizationGroup, arrow-function, async-functions, async-iteration, class, host-gc-required]
---*/

var iterator;
function poisoned(iter) {
  iterator = iter;
  iter.next(); // Won't throw
  throw new Test262Error();
}
var fg = new FinalizationGroup(poisoned);

function emptyCells() {
  (function() {
    var o = {};
    fg.register(o);
  })();
  $262.gc();
}

emptyCells();

assert.throws(Test262Error, function() {
  fg.cleanupSome();
});

assert.sameValue(typeof iteraror.next, 'function');
assert.throws(TypeError, function() {
  iterator.next();
}, 'iterator.next throws a TypeError if IsFinalizationGroupCleanupJobActive is false');
