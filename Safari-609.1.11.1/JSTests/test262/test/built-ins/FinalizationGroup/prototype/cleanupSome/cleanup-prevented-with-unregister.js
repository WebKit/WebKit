// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Cleanup might be prevented with an unregister usage
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.

  FinalizationGroup.prototype.unregister ( unregisterToken )

  1. Let removed be false.
  2. For each Record { [[Target]], [[Holdings]], [[UnregisterToken]] } cell that is an element of finalizationGroup.[[Cells]], do
    a. If SameValue(cell.[[UnregisterToken]], unregisterToken) is true, then
      i. Remove cell from finalizationGroup.[[Cells]].
      ii. Set removed to true.
  3. Return removed.
features: [FinalizationGroup, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var called = 0;
var fg = new FinalizationGroup(function() {});

function emptyCells() {
  var target = {};
  var token = {};
  fg.register(target, 'target!', token);

  var prom = asyncGC(target);
  target = null;

  var res = fg.unregister(token);
  assert.sameValue(res, true, 'unregister target before iterating over it in cleanup');

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(function cb(iterator) {
    called += 1;
  });
  
  assert.sameValue(called, 0, 'callback was not called');
}).then($DONE, resolveAsyncGC);
