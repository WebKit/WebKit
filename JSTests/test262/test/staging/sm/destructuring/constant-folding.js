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
    assertThrowsInstanceOf(function () { Function(code); }, SyntaxError, "Function:" + code);
    assertThrowsInstanceOf(function () { eval(code); }, SyntaxError, "eval:" + code);
    var ieval = eval;
    assertThrowsInstanceOf(function () { ieval(code); }, SyntaxError, "indirect eval:" + code);
}

// |true && a| is constant-folded to |a|, ensure the destructuring assignment
// validation takes place before constant-folding.
for (let prefix of ["null,", "var", "let", "const"]) {
    assertSyntaxError(`${prefix} [true && a] = [];`);
    assertSyntaxError(`${prefix} {p: true && a} = {};`);
}

