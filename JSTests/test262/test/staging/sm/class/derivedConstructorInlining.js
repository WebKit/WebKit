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
// Since we (for now!) can't emit jitcode for derived class statements. Make
// sure we can correctly invoke derived class constructors.

class foo extends null {
    constructor() {
        // Anything that tests |this| should throw, so just let it run off the
        // end.
    }
}

function intermediate() {
    new foo();
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(intermediate, "this");

