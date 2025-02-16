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
// @@unscopables prevents a property from having any effect on assigning to a
// const binding (which is an error).

const x = 1;
with ({x: 1, [Symbol.unscopables]: {x: true}})
    assertThrowsInstanceOf(() => {x = 2;}, TypeError);

