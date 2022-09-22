// Copyright (c) 2021 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.groupToMap
description: Array.prototype.groupToMap errors when accessing a getter throws.
info: |
  22.1.3.15 Array.prototype.groupToMap ( callbackfn [ , thisArg ] )

  ...

  6. Repeat, while k < len
    a. Let Pk be ! ToString(𝔽(k)).
    b. Let kValue be ? Get(O, Pk).

  ...
features: [array-grouping]
---*/

assert.throws(Test262Error, function() {
  const arrayLike = Object.defineProperty({
    length: 1,
  }, '0', {
    get: function() {
      throw new Test262Error('no element for you');
    }
  });
  Array.prototype.groupToMap.call(arrayLike, function() {
    return 'key';
  });
});
