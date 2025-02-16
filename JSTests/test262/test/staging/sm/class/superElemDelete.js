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
// Make sure we get the proper side effects.
// |delete super[expr]| applies ToPropertyKey on |expr| before throwing.

class base {
    constructor() { }
}

class derived extends base {
    constructor() { super(); }
    testDeleteElem() {
        let sideEffect = 0;
        let key = {
            toString() {
                sideEffect++;
                return "";
            }
        };
        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assert.sameValue(sideEffect, 0);
    }
}

class derivedTestDeleteElem extends base {
    constructor() {
        let sideEffect = 0;
        let key = {
            toString() {
                sideEffect++;
                return "";
            }
        };

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assert.sameValue(sideEffect, 0);

        super();

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assert.sameValue(sideEffect, 0);

        Object.setPrototypeOf(derivedTestDeleteElem.prototype, null);

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assert.sameValue(sideEffect, 0);

        return {};
    }
}

var d = new derived();
d.testDeleteElem();

new derivedTestDeleteElem();

