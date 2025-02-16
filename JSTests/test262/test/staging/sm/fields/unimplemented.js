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
// Field syntax doesn't crash the engine when fields are disabled.

// Are fields enabled?
let fieldsEnabled = false;
try {
  Function("class C { x; }");
  fieldsEnabled = true;
} catch (exc) {
  assert.sameValue(exc instanceof SyntaxError, true);
}

// If not, run these tests. (Many other tests cover actual behavior of the
// feature when enabled.)
if (!fieldsEnabled) {
  let source = `class C {
    x
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    x = 0;
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    0 = 0;
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    [0] = 0;
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    "hi" = 0;
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    "hi" = 0;
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    d = function(){};
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);

  source = `class C {
    d = class D { x = 0; };
  }`;
  assertThrowsInstanceOf(() => Function(source), SyntaxError);
}

