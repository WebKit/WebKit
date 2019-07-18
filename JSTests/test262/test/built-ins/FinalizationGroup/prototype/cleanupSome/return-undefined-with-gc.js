// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-finalization-group.prototype.cleanupSome
description: Return undefined regardless the result of CleanupFinalizationGroup
info: |
  FinalizationGroup.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationGroup be the this value.
  2. If Type(finalizationGroup) is not Object, throw a TypeError exception.
  3. If finalizationGroup does not have a [[Cells]] internal slot, throw a TypeError exception.
  4. If callback is not undefined and IsCallable(callback) is false, throw a TypeError exception.
  5. Perform ? CleanupFinalizationGroup(finalizationGroup, callback).
  6. Return undefined.
features: [FinalizationGroup, arrow-function, async-functions, async-iteration, class, host-gc-required]
---*/

var called;
var fn = function() {
  called += 1;
  return 39;
};
var cb = function() {
  called += 1;
  return 42;
};
var fg = new FinalizationGroup(fn);

function emptyCells() {
  called = 0;
  (function() {
    var o = {};
    fg.register(o);
  })();
  $262.gc();
}

emptyCells();
assert.sameValue(fg.cleanupSome(cb), undefined, 'regular callback');
assert.sameValue(called, 1);

emptyCells();
assert.sameValue(fg.cleanupSome(fn), undefined, 'regular callback, same FG cleanup function');
assert.sameValue(called, 1);

emptyCells();
assert.sameValue(fg.cleanupSome(() => 1), undefined, 'arrow function');

emptyCells();
assert.sameValue(fg.cleanupSome(fg.cleanupSome), undefined, 'cleanupSome itself');

emptyCells();
assert.sameValue(fg.cleanupSome(class {}), undefined, 'class expression');

emptyCells();
assert.sameValue(fg.cleanupSome(async function() {}), undefined, 'async function');

emptyCells();
assert.sameValue(fg.cleanupSome(function *() {}), undefined, 'generator');

emptyCells();
assert.sameValue(fg.cleanupSome(async function *() {}), undefined, 'async generator');

emptyCells();
assert.sameValue(fg.cleanupSome(), undefined, 'undefined, implicit');

emptyCells();
assert.sameValue(fg.cleanupSome(undefined), undefined, 'undefined, explicit');
