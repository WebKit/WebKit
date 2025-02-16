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
// Make sure duplicated name is allowed in non-strict.
function f0(a) {
}

// SyntaxError should be thrown if method definition has duplicated name.
assertThrowsInstanceOf(() => eval(`
({
  m1(a, a) {
  }
});
`), SyntaxError);
assertThrowsInstanceOf(() => eval(`
({
  m2(a, ...a) {
  }
});
`), SyntaxError);

