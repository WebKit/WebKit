// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  FinalizationRegistryCleanupIteratorPrototype @@toStringTag
info: |
  FinalizationRegistry.prototype.cleanupSome ( [ callback ] )

  1. Let finalizationRegistry be the this value.
  ...
  5. Perform ! CleanupFinalizationRegistry(finalizationRegistry, callback).
  ...

  CleanupFinalizationRegistry ( finalizationRegistry [ , callback ] )

  ...
  3. Let iterator be ! CreateFinalizationRegistryCleanupIterator(finalizationRegistry).
  ...
  6. Let result be Call(callback, undefined, « iterator »).
  ...

  %FinalizationRegistryCleanupIteratorPrototype% [ @@toStringTag ]

  The initial value of the @@toStringTag property is the String value "FinalizationRegistry Cleanup Iterator".

  This property has the attributes { [[Writable]]: false, [[Enumerable]]: false, [[Configurable]]: true }.
features: [FinalizationRegistry, host-gc-required, Symbol, Symbol.toStringTag]
includes: [async-gc.js, propertyHelper.js]
flags: [async, non-deterministic]
---*/

var FinalizationRegistryCleanupIteratorPrototype;
var called = 0;
var finalizationRegistry = new FinalizationRegistry(function() {});

function callback(iterator) {
  called += 1;
  FinalizationRegistryCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

function emptyCells() {
  var target = {};
  finalizationRegistry.register(target);

  var prom = asyncGC(target);
  target = null;

  return prom;
}

emptyCells().then(function() {
  finalizationRegistry.cleanupSome(callback);

  assert.sameValue(called, 1, 'cleanup successful');
  
  verifyProperty(FinalizationRegistryCleanupIteratorPrototype, Symbol.toStringTag, {
    value: 'FinalizationRegistry Cleanup Iterator',
    writable: false,
    enumerable: false,
    configurable: true
  });  
}).then($DONE, resolveAsyncGC);
