// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  FinalizationGroupCleanupIteratorPrototype.next.length property descriptor
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

  17 ECMAScript Standard Built-in Objects

  Every built-in function object, including constructors, has a length
  property whose value is an integer. Unless otherwise specified, this
  value is equal to the largest number of named arguments shown in the
  subclause headings for the function description. Optional parameters
  (which are indicated with brackets: [ ]) or rest parameters (which
  are shown using the form «...name») are not included in the default
  argument count.

  Unless otherwise specified, the length property of a built-in
  function object has the attributes { [[Writable]]: false,
  [[Enumerable]]: false, [[Configurable]]: true }.
includes: [async-gc.js, propertyHelper.js]
flags: [async, non-deterministic]
features: [FinalizationGroup, host-gc-required, Symbol]
---*/

var FinalizationGroupCleanupIteratorPrototype;
var called = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;
  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
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

  assert.sameValue(typeof FinalizationGroupCleanupIteratorPrototype.next, 'function');

  verifyProperty(FinalizationGroupCleanupIteratorPrototype.next, 'length', {
    value: 0,
    enumerable: false,
    writable: false,
    configurable: true,
  });
}).then($DONE, resolveAsyncGC);
