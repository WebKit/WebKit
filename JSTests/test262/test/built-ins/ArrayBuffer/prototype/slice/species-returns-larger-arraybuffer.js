// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer.prototype.slice
description: >
  Does not throw TypeError if new ArrayBuffer is too large.
info: |
  ArrayBuffer.prototype.slice ( start, end )

  ...
  13. Let ctor be SpeciesConstructor(O, %ArrayBuffer%).
  14. ReturnIfAbrupt(ctor).
  15. Let new be Construct(ctor, «newLen»).
  16. ReturnIfAbrupt(new).
  ...
  20. If the value of new’s [[ArrayBufferByteLength]] internal slot < newLen, throw a TypeError exception.
  ...
features: [Symbol.species]
---*/

var speciesConstructor = {};
speciesConstructor[Symbol.species] = function(length) {
  return new ArrayBuffer(10);
};

var arrayBuffer = new ArrayBuffer(8);
arrayBuffer.constructor = speciesConstructor;

var result = arrayBuffer.slice();
assert.sameValue(result.byteLength, 10);
