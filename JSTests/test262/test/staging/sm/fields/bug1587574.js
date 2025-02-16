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
// Don't Crash
var testStr = `
class C extends Object {
  constructor() {
    eval(\`a => b => {
      class Q { f = 1; }  // inhibits lazy parsing
      super();
    }\`);
  }
}
new C;`
assert.sameValue(raisesException(ReferenceError)(testStr), true);

