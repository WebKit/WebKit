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
class base {
    constructor() { }
    test() {
        return false;
    }
}

let standin = { test() { return true; } };

class derived extends base {
    constructor() { super(); }
    test() {
        assert.sameValue(super.test(), false);
        Object.setPrototypeOf(derived.prototype, standin);
        assert.sameValue(super["test"](), true);
    }
}

new derived().test();

