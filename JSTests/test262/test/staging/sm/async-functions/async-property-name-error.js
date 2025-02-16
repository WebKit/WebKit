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
function assertSyntaxError(code) {
    assertThrowsInstanceOf(() => { Function(code); }, SyntaxError, "Function:" + code);
    assertThrowsInstanceOf(() => { eval(code); }, SyntaxError, "eval:" + code);
    var ieval = eval;
    assertThrowsInstanceOf(() => { ieval(code); }, SyntaxError, "indirect eval:" + code);
}

assertSyntaxError(`({async async: 0})`);
assertSyntaxError(`({async async})`);
assertSyntaxError(`({async async, })`);
assertSyntaxError(`({async async = 0} = {})`);

for (let decl of ["var", "let", "const"]) {
    assertSyntaxError(`${decl} {async async: a} = {}`);
    assertSyntaxError(`${decl} {async async} = {}`);
    assertSyntaxError(`${decl} {async async, } = {}`);
    assertSyntaxError(`${decl} {async async = 0} = {}`);
}

