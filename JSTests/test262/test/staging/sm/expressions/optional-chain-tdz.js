// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Test TDZ for optional chaining.

// TDZ for lexical |let| bindings with optional chaining.
{
  assertThrowsInstanceOf(() => {
    const Null = null;
    Null?.[b];
    b = 0;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const Null = null;
    Null?.[b]();
    b = 0;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const Null = null;
    delete Null?.[b];
    b = 0;
    let b;
  }, ReferenceError);
}

