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
        assert.sameValue(eval("super(); this"), this);
        assert.sameValue(this, eval("this"));
        assert.sameValue(this, (()=>this)());
    }
}();

new class extends class { } {
    constructor() {
        (()=>super())();
        assert.sameValue(this, eval("this"));
        assert.sameValue(this, (()=>this)());
    }
}();

