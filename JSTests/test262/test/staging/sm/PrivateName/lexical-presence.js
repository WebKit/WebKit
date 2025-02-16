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

// Verify that private fields are enabled.
class A {
  #x;
};

function assertThrowsSyntaxError(str) {
  assertThrowsInstanceOf(() => eval(str), SyntaxError);       // Direct Eval
  assertThrowsInstanceOf(() => (1, eval)(str), SyntaxError);  // Indirect Eval
  assertThrowsInstanceOf(() => Function(str), SyntaxError);   // Function
}

assertThrowsSyntaxError(`
    class B {
        g() {
            return this.#x;
        }
    };
`);

assertThrowsSyntaxError(`
with (new class A { #x; }) {
    class B {
      #y;
      constructor() { return this.#x; }
    }
  }
`);

// Make sure we don't create a generic binding for #x.
with (new class {
  #x = 12;
}) {
  assert.sameValue('#x' in this, false);
}

// Try to fetch a non-existing field using different
// compilation contexts
function assertNonExisting(fetchCode) {
  source = `var X = class {
    b() {
      ${fetchCode}
    }
  }
  var a = new X;
  a.b()`
  assertThrowsInstanceOf(() => eval(source), SyntaxError);
}

assertNonExisting(`return eval("this.#x")"`);
assertNonExisting(`return (1,eval)("this.#x")`);
assertNonExisting(`var f = Function("this.#x"); return f();`);

