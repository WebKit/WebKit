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

assertThrowsInstanceOf(() => eval(`class A { #x; #x; }`), SyntaxError);

// No computed private fields
assertThrowsInstanceOf(
    () => eval(`var x = "foo"; class A { #[x] = 20; }`), SyntaxError);

assertThrowsInstanceOfWithMessage(() => eval(`class A { #x; h(o) { return !#x; }}`),
    SyntaxError,
    "invalid use of private name in unary expression without object reference");
assertThrowsInstanceOfWithMessage(() => eval(`class A { #x; h(o) { return 1 + #x in o; }}`),
    SyntaxError,
    "invalid use of private name due to operator precedence");


