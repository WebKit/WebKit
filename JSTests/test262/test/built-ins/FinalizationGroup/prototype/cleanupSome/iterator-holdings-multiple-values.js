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
includes: [compareArray.js]
---*/

var holdings;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  holdings = [...iterator];
}

(function() {
  var o = {};
  fg.register(o);
  fg.register(o, undefined);
})();

$262.gc();
fg.cleanupSome(callback);

assert.compareArray(holdings, [undefined, undefined], 'undefined');

(function() {
  var o = {};
  fg.register(o, null);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [null], 'null');

(function() {
  var o = {};
  fg.register(o, '');
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [''], 'the empty string');

var other = {};
(function() {
  var o = {};
  fg.register(o, other);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [other], '{}');

(function() {
  var o = {};
  fg.register(o, 42);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [42], '42');

(function() {
  var o = {};
  fg.register(o, true);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [true], 'true');

(function() {
  var o = {};
  fg.register(o, false);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [false], 'false');

var s = Symbol();
(function() {
  var o = {};
  fg.register(o, s);
})();

$262.gc();
fg.cleanupSome(callback);
assert.compareArray(holdings, [s], 'symbol');
