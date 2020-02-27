// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-properties-of-the-finalization-registry-constructor
description: >
  Prop descriptor for FinalizationRegistryCleanupIteratorPrototype.next
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

  17 ECMAScript Standard Built-in Objects:

  Every other data property described in clauses 18 through 26 and in Annex B.2
  has the attributes { [[Writable]]: true, [[Enumerable]]: false,
  [[Configurable]]: true } unless otherwise specified.
includes: [propertyHelper.js, async-gc.js]
flags: [async, non-deterministic]
features: [FinalizationRegistry, host-gc-required, Symbol]
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

  assert.sameValue(typeof FinalizationRegistryCleanupIteratorPrototype.next, 'function');

  var next = FinalizationRegistryCleanupIteratorPrototype.next;

  verifyProperty(FinalizationRegistryCleanupIteratorPrototype, 'next', {
    enumerable: false,
    writable: true,
    configurable: true,
  });
}).then($DONE, resolveAsyncGC);
