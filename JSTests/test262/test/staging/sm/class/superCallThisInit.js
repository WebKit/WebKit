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
function base() { this.prop = 42; }

class testInitialize extends base {
    constructor() {
        // A poor man's assertThrowsInstanceOf, as arrow functions are currently
        // disabled in this context
        try {
            this;
            throw new Error();
        } catch (e) {
            if (!(e instanceof ReferenceError))
                throw e;
        }
        super();
        assert.sameValue(this.prop, 42);
    }
}
assert.sameValue(new testInitialize().prop, 42);

// super() twice is a no-go.
class willThrow extends base {
    constructor() {
        super();
        super();
    }
}
assertThrowsInstanceOf(()=>new willThrow(), ReferenceError);

// This is determined at runtime, not the syntax level.
class willStillThrow extends base {
    constructor() {
        for (let i = 0; i < 3; i++) {
            super();
        }
    }
}
assertThrowsInstanceOf(()=>new willStillThrow(), ReferenceError);

class canCatchThrow extends base {
    constructor() {
        super();
        try { super(); } catch(e) { }
    }
}
assert.sameValue(new canCatchThrow().prop, 42);

