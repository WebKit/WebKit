// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Cleanup might be prevented with a reference usage;
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.
features: [FinalizationGroup, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var holdingsList;
function cb(iterator) {
  holdingsList = [...iterator];
};
var fg = new FinalizationGroup(function() {});

var referenced = {};

function emptyCells() {
  var target = {};
  fg.register(target, 'target!');
  fg.register(referenced, 'referenced');

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(cb);

  assert.sameValue(holdingsList.length, 1);
  assert.sameValue(holdingsList[0], 'target!');

  assert.sameValue(typeof referenced, 'object', 'referenced preserved');
}).then($DONE, resolveAsyncGC);
