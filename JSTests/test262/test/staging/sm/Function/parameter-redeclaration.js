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
// 'var' is allowed to redeclare parameters.
function f1(a = 0) {
  var a;
}

// 'let' and 'const' at body-level are not allowed to redeclare parameters.
assertThrowsInstanceOf(() => {
  eval(`function f2(a = 0) {
    let a;
  }`);
}, SyntaxError);
assertThrowsInstanceOf(() => {
  eval(`function f3(a = 0) {
    const a;
  }`);
}, SyntaxError);

