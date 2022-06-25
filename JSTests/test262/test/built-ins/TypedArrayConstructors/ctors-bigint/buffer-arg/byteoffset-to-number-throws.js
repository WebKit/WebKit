// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-typedarray-buffer-byteoffset-length
description: >
  Return abrupt from parsing integer value from byteOffset
info: |
  22.2.4.5 TypedArray ( buffer [ , byteOffset [ , length ] ] )

  This description applies only if the TypedArray function is called with at
  least one argument and the Type of the first argument is Object and that
  object has an [[ArrayBufferData]] internal slot.

  ...
  7. Let offset be ? ToInteger(byteOffset).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var buffer = new ArrayBuffer(8);
var byteOffset = {
  valueOf: function() {
    throw new Test262Error();
  }
};

testWithBigIntTypedArrayConstructors(function(TA) {
  assert.throws(Test262Error, function() {
    new TA(buffer, byteOffset);
  });
});
