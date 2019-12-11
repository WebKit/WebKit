// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  If iterator does not have a [[FinalizationGroup]] internal slot, throw a TypeError exception.
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

  1. Let iterator be the this value.
  2. If Type(iterator) is not Object, throw a TypeError exception.
  3. If iterator does not have a [[FinalizationGroup]] internal slot, throw a TypeError exception.
features: [FinalizationGroup, WeakRef, host-gc-required, Symbol]
includes: [async-gc.js]
flags: [async, non-deterministic]
---*/

var FinalizationGroupCleanupIteratorPrototype;
var called = 0;
var endOfCall = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;
  // Only the iterator itself will have a [[FinalizationGroup]] internal
  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);

  // It's sensitive the assertions remain inside this function in order to secure
  // [[IsFinalizationGroupCleanupJobActive]] is true
  assert.sameValue(typeof FinalizationGroupCleanupIteratorPrototype.next, 'function');

  var next = FinalizationGroupCleanupIteratorPrototype.next;

  assert.throws(TypeError, function() {
    next.call({});
  }, '{}');

  assert.throws(TypeError, function() {
    next.call(FinalizationGroup);
  }, 'FinalizationGroup');

  assert.throws(TypeError, function() {
    next.call(FinalizationGroupCleanupIteratorPrototype);
  }, 'FinalizationGroupCleanupIteratorPrototype');

  assert.throws(TypeError, function() {
    next.call(fg);
  }, 'FinalizationGroup instance');

  var wr = new WeakRef({});
  assert.throws(TypeError, function() {
    next.call(wr);
  }, 'WeakRef instance');

  // Abrupt completions are not directly returned.
  endOfCall += 1;
}

function emptyCells() {
  var target = {};
  fg.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  fg.cleanupSome(callback);

  assert.sameValue(called, 1, 'cleanup successful');
  assert.sameValue(endOfCall, 1, 'Abrupt completions are not directly returned.');
}).then($DONE, resolveAsyncGC);
