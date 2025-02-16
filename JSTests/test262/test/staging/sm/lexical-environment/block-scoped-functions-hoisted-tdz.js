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
var log = "";
try {
  (function() {
    {
      let y = f();
      function f() { y; }
    }
  })()
} catch (e) {
  log += e instanceof ReferenceError;
}

try {
  function f() {
    switch (1) {
      case 0:
        let x;
      case 1:
        (function() { x; })();
    }
  }
  f();
} catch (e) {
  log += e instanceof ReferenceError;
}

assert.sameValue(log, "truetrue");

