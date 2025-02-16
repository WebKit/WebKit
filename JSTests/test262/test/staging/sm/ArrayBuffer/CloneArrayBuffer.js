// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1264941;
var summary = 'CloneArrayBuffer should be called with byteLength of source typedArray';

print(BUGNUMBER + ": " + summary);

function test(ctor, byteLength) {
  var abuf = new ctor(byteLength);
  assert.sameValue(abuf.byteLength, byteLength);

  for (var byteOffset of [0, 16]) {
    for (var elementLength = 0;
         elementLength < (byteLength - byteOffset) / Float64Array.BYTES_PER_ELEMENT;
         elementLength++) {
      var a1 = new Float64Array(abuf, byteOffset, elementLength);
      assert.sameValue(a1.buffer.byteLength, byteLength);
      assert.sameValue(a1.byteLength, elementLength * Float64Array.BYTES_PER_ELEMENT);
      assert.sameValue(a1.byteOffset, byteOffset);

      var a2 = new Float64Array(a1);
      assert.sameValue(a2.buffer.byteLength, a1.byteLength);
      assert.sameValue(a2.byteLength, a1.byteLength);
      assert.sameValue(a2.byteOffset, 0);
    }
  }
}

test(ArrayBuffer, 16);
test(ArrayBuffer, 128);

class MyArrayBuffer extends ArrayBuffer {}
test(MyArrayBuffer, 16);
test(MyArrayBuffer, 128);

