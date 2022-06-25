// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.from
description: Returns error produced by advancing the iterator
info: |
  22.2.2.1.1 Runtime Semantics: IterableToArrayLike( items )

  2. If usingIterator is not undefined, then
    ...
    d. Repeat, while next is not false
      i. Let next be ? IteratorStep(iterator).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, Symbol.iterator, TypedArray]
---*/

var iter = {};
iter[Symbol.iterator] = function() {
  return {
    next: function() {
      throw new Test262Error();
    }
  };
};

testWithBigIntTypedArrayConstructors(function(TA) {
  assert.throws(Test262Error, function() {
    TA.from(iter);
  });
});
