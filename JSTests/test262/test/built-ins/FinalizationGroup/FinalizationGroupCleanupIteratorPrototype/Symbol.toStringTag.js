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
includes: [propertyHelper.js]
features: [FinalizationGroup, host-gc-required, Symbol, Symbol.toStringTag]
---*/

var FinalizationGroupCleanupIteratorPrototype;
var called = 0;
var fg = new FinalizationGroup(function() {});

function callback(iterator) {
  called += 1;
  FinalizationGroupCleanupIteratorPrototype = Object.getPrototypeOf(iterator);
}

(function() {
  var o = {};
  fg.register(o);
})();

$262.gc();

fg.cleanupSome(callback);

assert.sameValue(called, 1, 'cleanup successful');

verifyProperty(FinalizationGroupCleanupIteratorPrototype, Symbol.toStringTag, {
  value: 'FinalizationGroup Cleanup Iterator',
  writable: false,
  enumerable: false,
  configurable: true
});
