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

var ta = new Int32Array([3, 2, 1]);

$262.detachArrayBuffer(ta.buffer);

assertThrowsInstanceOf(() => ta.toSorted(), TypeError);

