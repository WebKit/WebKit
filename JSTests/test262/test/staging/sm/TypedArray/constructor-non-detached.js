// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (var constructor of typedArrayConstructors) {
    var buf = new constructor();
    $262.detachArrayBuffer(buf.buffer);
    assertThrowsInstanceOf(() => new constructor(buf), TypeError);

    var buffer = new ArrayBuffer();
    $262.detachArrayBuffer(buffer);
    assertThrowsInstanceOf(() => new constructor(buffer), TypeError);
}


