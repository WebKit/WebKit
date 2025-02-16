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
new class extends class { } {
    constructor() {
        let a1 = () => this;
        let a2 = (() => super());
        assertThrowsInstanceOf(a1, ReferenceError);
        assert.sameValue(a2(), a1());
    }
}();

