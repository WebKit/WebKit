// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262-TypedArray-shell.js]
description: |
  pending
esid: pending
---*/
for (var constructor of typedArrayConstructors) {
    var buf = new constructor();
    $262.detachArrayBuffer(buf.buffer);
    assert.throws(TypeError, () => new constructor(buf));

    var buffer = new ArrayBuffer();
    $262.detachArrayBuffer(buffer);
    assert.throws(TypeError, () => new constructor(buffer));
}


