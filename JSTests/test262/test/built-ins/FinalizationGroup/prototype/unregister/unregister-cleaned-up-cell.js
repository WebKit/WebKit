// Copyright (C) 2019 Mathieu Hofman. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.unregister
description: Cannot unregister a cell that has been cleaned up
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  ...
  5. Perform ! CleanupFinalizationGroup(finalizationGroup, callback).
  ...

  CleanupFinalizationGroup ( finalizationGroup [ , callback ] )

  ...
  3. Let iterator be ! CreateFinalizationGroupCleanupIterator(finalizationGroup).
  ...
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  %FinalizationGroupCleanupIteratorPrototype%.next ( )

  8. If finalizationGroup.[[Cells]] contains a Record cell such that cell.[[Target]] is empty,
    a. Choose any such cell.
    b. Remove cell from finalizationGroup.[[Cells]].
    c. Return CreateIterResultObject(cell.[[Holdings]], false).
  9. Otherwise, return CreateIterResultObject(undefined, true).

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

var value = 'target!';
var token = {};
var fg = new FinalizationGroup(function() {});

function emptyCells() {
  var target = {};
  fg.register(target, value, token);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  var called = 0;
  var holdings;
  fg.cleanupSome(function cb(iterator) {
    called += 1;
    holdings = [...iterator];
  });

  assert.sameValue(called, 1);
  assert.sameValue(holdings.length, 1);
  assert.sameValue(holdings[0], value);

  var res = fg.unregister(token);
  assert.sameValue(res, false, 'unregister after iterating over it in cleanup');
}).then($DONE, resolveAsyncGC);
