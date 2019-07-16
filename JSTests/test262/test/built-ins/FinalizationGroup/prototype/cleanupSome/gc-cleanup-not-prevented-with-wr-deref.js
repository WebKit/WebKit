// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: WeakRef deref should not prevent a GC event
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  ...
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.

  WeakRef.prototype.deref ( )

  ...
  4. Let target be the value of weakRef.[[Target]].
  5. If target is not empty,
    a. Perform ! KeepDuringJob(target).
    b. Return target.
  6. Return undefined.
features: [FinalizationGroup, WeakRef, host-gc-required]
---*/

var holdingsList;
function cb(iterator) {
  holdingsList = [...iterator];
}
var fg = new FinalizationGroup(function() {});

var deref = false;

function emptyCells() {
  var wr;
  (function() {
    var a = {};
    wr = new WeakRef(a);
    fg.register(a, 'a');
  })();
  $262.gc();
  deref = wr.deref();
}

emptyCells();
fg.cleanupSome(cb);

assert.sameValue(holdingsList.length, 1);
assert.sameValue(holdingsList[0], 'a');

assert.sameValue(deref, undefined, 'deref handles an empty wearRef.[[Target]] returning undefined');
