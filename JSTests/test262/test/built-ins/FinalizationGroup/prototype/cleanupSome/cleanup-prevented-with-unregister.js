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
---*/

var holdingsList;
var fg = new FinalizationGroup(function() {});

var unregA = {};
var unregB = {};
var unregC = {};
var unregDE = {}; // Use the same unregister token for D and E

function emptyCells() {
  (function() {
    var a = {};
    var b = {};
    var c = {};
    var d = {};
    var e = {};
    fg.register(a, 'a', unregA);
    fg.register(b, 'b', unregB);
    fg.register(c, 'c', unregC);
    fg.register(d, 'd', unregDE);
    fg.register(e, 'e', unregDE);
  })();

  var res = fg.unregister(unregC); // unregister 'c' before GC
  assert.sameValue(res, true, 'unregister c before GC');

  $262.gc();
}

emptyCells();

var res = fg.unregister(unregDE);
assert.sameValue(res, true, 'unregister d and e after GC');

fg.cleanupSome(function cb(iterator) {
  var res = fb.unregister(unregA);
  assert.sameValue(res, true, 'unregister a before the iterator is consumed.');

  holdingsList = [...iterator];

  // It's not possible to verify an unregister during the iterator consumption
  // as it's .next() does not have any specific order to follow for each item.
});

assert.sameValue(holdingsList[0], 'b');
assert.sameValue(holdingsList.length, 1);

// Second run
res = fg.unregister(unregB); // let's empty the cells
assert.sameValue(res, true, 'unregister B for cleanup');
holdingsList = undefined;
emptyCells();

fg.cleanupSome(function cb(iterator) {
  var res = fb.unregister(unregDE);
  assert.sameValue(res, true, 'unregister d and e before the iterator is consumed.');
  holdingsList = [...iterator];
});

assert(holdingsList.includes('b'));
assert(holdingsList.includes('a'), 'just like the first run, now without removing a');
assert.sameValue(holdingsList.length, 2);
