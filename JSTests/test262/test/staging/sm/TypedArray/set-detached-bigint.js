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
let ta = new BigInt64Array(10);

let obj = {
  get length() {
    $262.detachArrayBuffer(ta.buffer);
    return 1;
  },
  0: {
    valueOf() {
      return "huzzah!";
    }
  },
};

// Throws a SyntaxError, because "huzzah!" can't be parsed as a BigInt.
assertThrowsInstanceOf(() => ta.set(obj), SyntaxError);

