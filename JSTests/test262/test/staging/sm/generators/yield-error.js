// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1384299;
var summary = "yield outside of generators should provide better error";

print(BUGNUMBER + ": " + summary);

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

assertThrowsInstanceOfWithMessage(
    () => eval("yield 10"),
    SyntaxError,
    "yield expression is only valid in generators");

