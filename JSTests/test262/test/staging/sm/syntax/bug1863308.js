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
assertThrowsInstanceOfWithMessage(
    () => eval("for (let case of ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'case'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let debugger of ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'debugger'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let case in ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'case'");

assertThrowsInstanceOfWithMessage(
    () => eval("for (let debugger in ['foo', 'bar']) {}"),
    SyntaxError,
    "unexpected token: keyword 'debugger'");

