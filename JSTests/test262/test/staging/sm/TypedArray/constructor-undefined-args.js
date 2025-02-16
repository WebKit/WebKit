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
// Bug 1040402

var ab = new ArrayBuffer(16);

assert.sameValue(new Int32Array(ab).length, 4);
assert.sameValue(new Int32Array(ab, undefined).length, 4);
assert.sameValue(new Int32Array(ab, undefined, undefined).length, 4);
assert.sameValue(new Int32Array(ab, 0).length, 4);
assert.sameValue(new Int32Array(ab, 0, undefined).length, 4);
assert.sameValue(new Int32Array(ab, 4).length, 3);
assert.sameValue(new Int32Array(ab, 4, undefined).length, 3);

