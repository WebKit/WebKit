// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-group-constructor
description: >
  FinalizationGroupCleanupIteratorPrototype @@toStringTag
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

  %FinalizationGroupCleanupIteratorPrototype% [ @@toStringTag ]

  The initial value of the @@toStringTag property is the String value "FinalizationGroup Cleanup Iterator".

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
features: [FinalizationGroup, host-gc-required, Symbol, Symbol.toStringTag]
includes: [async-gc.js, propertyHelper.js]
flags: [async, non-deterministic]
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
  
  verifyProperty(FinalizationGroupCleanupIteratorPrototype, Symbol.toStringTag, {
    value: 'FinalizationGroup Cleanup Iterator',
    writable: false,
    enumerable: false,
    configurable: true
  });  
}).then($DONE, resolveAsyncGC);
