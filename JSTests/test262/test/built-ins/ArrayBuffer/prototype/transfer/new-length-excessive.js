// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-arraybuffer.prototype.transfer
description: >
  Throws a RangeError the newLength value is too large to create a new
  ArrayBuffer.
info: |
  ArrayBuffer.prototype.transfer ( [ newLength ] )

  1. Let O be the this value.
  2. Perform ? RequireInternalSlot(O, [[ArrayBufferData]]).
  3. If IsSharedArrayBuffer(O) is true, throw a TypeError exception.
  4. If IsDetachedBuffer(O) is true, throw a TypeError exception.
  5. If newLength is undefined, let newByteLength be
     O.[[ArrayBufferByteLength]].
  6. Else, let newByteLength be ? ToIntegerOrInfinity(newLength).
  7. Let new be ? Construct(%ArrayBuffer%, « 𝔽(newByteLength) »).
  [...]
features: [resizable-arraybuffer]
---*/

var ab = new ArrayBuffer(0);

assert.throws(RangeError, function() {
  // Math.pow(2, 53) = 9007199254740992
  ab.transfer(9007199254740992);
});
