// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  The callback iterator is dynamic. Therefore, it can be emptied before iteration
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
features: [FinalizationGroup, host-gc-required, Symbol]
---*/

var called = 0;
var endOfCall = 0;
var first, second, third;
var firstIter, secondIter, thirdIter;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;

  if (called === 1) {

    // Inception!
    fg.cleanupSome(callback);

    first = iterator.next();
    firstIter = iterator;
  } else if (called === 2) {

    // Double Inception!
    fg.cleanupSome(callback);

    second = iterator.next();
    secondIter = iterator;
  } else if (called === 3) {

    // Triple Inception!
    // But this time we won't try to consume iterator anymore, leave it alone there.
    fg.cleanupSome(callback);

    third = iterator.next();
    thirdIter = iterator;
  }

  endOfCall += 1;
}

(function() {
  var o1 = {};
  var o2 = {};
  fg.register(o1, 'holdings 1');
  fg.register(o2, 'holdings 2');
})();

$262.gc();

fg.cleanupSome(callback);

// Make sure everything is set
assert.sameValue(called, 4, 'cleanup successfully');
assert.sameValue(endOfCall, 4, 'cleanup ended successfully');

assert.notSameValue(firstIter, secondIter, 'callback is not called with the same iterator #1');
assert.notSameValue(firstIter, thirdIter, 'callback is not called with the same iterator #2');
assert.notSameValue(secondIter, thirdIter, 'callback is not called with the same iterator #3');

assert.sameValue(first.value, undefined, 'iterator is already consumed');
assert.sameValue(first.done, true, 'first callback will find no empty Targets');
assert.sameValue(second.done, false, 'second callback will find an empty Target');
assert.sameValue(third.done, false, 'third callback will find an empty Target');

// 8.a. Choose any such cell.
var holdings = [second.value, third.value];

assert(holdings.includes('holdings 1'), 'iterators consume emptied cells #1');
assert(holdings.includes('holdings 2'), 'iterators consume emptied cells #2');
