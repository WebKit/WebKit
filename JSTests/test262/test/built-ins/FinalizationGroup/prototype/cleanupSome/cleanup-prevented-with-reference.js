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
---*/

var holdingsList;
function cb(iterator) {
  holdingsList = [...iterator];
};
var fg = new FinalizationGroup(function() {});

function emptyCells() {
  var x;
  (function() {
    var a = {};
    b = {};
    x = {};
    var y = {};
    fg.register(x, 'x');
    fg.register(a, 'a');
    fg.register(y, y);
    fg.register(b, 'b');
    var b;
  })();
  $262.gc();
  x; // This is an intentional reference to x, please leave it here, after the GC
}

emptyCells();
fg.cleanupSome(cb);

// The order is per implementation so we can't use compareArray without sorting
assert.sameValue(holdingsList.length, 2);
assert(holdingsList.includes('a'));
assert(holdingsList.includes('b'));
