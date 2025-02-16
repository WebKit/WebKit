// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
info: |
  needs shell functions
description: |
  pending
esid: pending
---*/

var b = createExternalArrayBuffer(0);
assert.sameValue(b.byteLength, 0);

