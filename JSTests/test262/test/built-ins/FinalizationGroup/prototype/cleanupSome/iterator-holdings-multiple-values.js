// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  Iterates over different type values in holdings
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
features: [FinalizationGroup, Symbol, host-gc-required]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

function check(value, expectedName) {
  var holdings;
  var called = 0;
  var fg = new FinalizationGroup(function() {});

  function callback(iterator) {
    called += 1;
    holdings = [...iterator];
  }

  // This is internal to avoid conflicts
  function emptyCells(value) {
    var target = {};
    fg.register(target, value);

    var prom = asyncGC(target);
    target = null;

    return prom;
  }

  return emptyCells(value).then(function() {
    fg.cleanupSome(callback);
    assert.sameValue(called, 1, expectedName);
    assert.sameValue(holdings.length, 1, expectedName);
    assert.sameValue(holdings[0], value, expectedName);
  });
}

Promise.all([
  check(undefined, 'undefined'),
  check(null, 'null'),
  check('', 'the empty string'),
  check({}, 'object'),
  check(42, 'number'),
  check(true, 'true'),
  check(false, 'false'),
  check(Symbol(1), 'symbol'),
]).then(() => $DONE(), resolveAsyncGC);
