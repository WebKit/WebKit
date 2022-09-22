// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap errors when return value cannot be converted to a property key.
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    c. Let propertyKey be ? ToPropertyKey(? Call(callbackfn, thisArg, « kValue, 𝔽(k), O »)).

  ...
features: [array-grouping]
---*/

assert.throws(Test262Error, function() {
  const array = [1];
  array.groupToMap(function() {
    throw new Test262Error('throw in callback');
  })
});
