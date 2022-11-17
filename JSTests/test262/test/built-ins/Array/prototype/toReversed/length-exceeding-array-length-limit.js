// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.toReversed
description: >
  Array.prototype.toReversed limits the length to 2 ** 32 - 1
info: |
  Array.prototype.toReversed ( )

  ...
  2. Let len be ? LengthOfArrayLike(O).
  3. Let A be ? ArrayCreate(𝔽(len)).
  ...

  ArrayCreate ( length [, proto ] )

  1. If length > 2 ** 32 - 1, throw a RangeError exception.
features: [change-array-by-copy]
---*/

// Object with large "length" property
var arrayLike = {
  get "0"() {
    throw new Test262Error("Get 0");
  },
  get "4294967295" () { // 2 ** 32 - 1
    throw new Test262Error("Get 2147483648");
  },
  get "4294967296" () { // 2 ** 32
    throw new Test262Error("Get 2147483648");
  },
  length: 2 ** 32
};

assert.throws(RangeError, function() {
  Array.prototype.toReversed.call(arrayLike);
});
