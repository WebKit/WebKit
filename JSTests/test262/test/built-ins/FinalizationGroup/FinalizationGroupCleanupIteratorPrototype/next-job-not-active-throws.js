// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  Throws a TypeError if [[IsFinalizationGroupCleanupJobActive]] is false
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
  5. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to true.
  6. Let result be Call(callback, undefined, « iterator »).
  7. Set finalizationGroup.[[IsFinalizationGroupCleanupJobActive]] to false.
  ...

  %FinalizationGroupCleanupIteratorPrototype%.next ( )

  1. Let iterator be the this value.
  2. If Type(iterator) is not Object, throw a TypeError exception.
  3. If iterator does not have a [[FinalizationGroup]] internal slot, throw a TypeError exception.
features: [FinalizationGroup, WeakRef, host-gc-required, Symbol]
---*/

var iter;
var FinalizationGroupCleanupIteratorPrototype;
var called = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;
  iter = iterator;
  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

(function() {
  var o = {};
  fg.register(o);
})();

$262.gc();

fg.cleanupSome(callback);

// Make sure everything is set
assert.sameValue(called, 1, 'cleanup successful');
assert.sameValue(typeof iter, 'object');
assert.sameValue(Object.getPrototypeOf(iter), FinalizationGroupCleanupIteratorPrototype);

// To the actual assertion
assert.throws(TypeError, function() {
  iter.next();
}, 'Iter should fail if not called during the cleanupSome call');
