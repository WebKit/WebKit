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
// |delete super.prop| and |delete super[expr]| throw universally.

class base {
    constructor() { }
}

class derived extends base {
    constructor() { super(); }
    testDeleteProp() { delete super.prop; }
    testDeleteElem() {
        let sideEffect = 0;
        assertThrowsInstanceOf(() => delete super[sideEffect = 1], ReferenceError);
        assert.sameValue(sideEffect, 1);
    }
}

var d = new derived();
assertThrowsInstanceOf(() => d.testDeleteProp(), ReferenceError);
d.testDeleteElem();

// |delete super.x| does not delete anything before throwing.
var thing1 = {
    go() { delete super.toString; }
};
let saved = Object.prototype.toString;
assertThrowsInstanceOf(() => thing1.go(), ReferenceError);
assert.sameValue(Object.prototype.toString, saved);

// |delete super.x| does not tell the prototype to delete anything, when it's a proxy.
var thing2 = {
    go() { delete super.prop; }
};
Object.setPrototypeOf(thing2, new Proxy({}, {
    deleteProperty(x) { throw "FAIL"; }
}));
assertThrowsInstanceOf(() => thing2.go(), ReferenceError);

class derivedTestDeleteProp extends base {
    constructor() {
        // The deletion error is a reference error, even after munging the prototype
        // chain.
        Object.setPrototypeOf(derivedTestDeleteProp.prototype, null);

        assertThrowsInstanceOf(() => delete super.prop, ReferenceError);

        super();

        assertThrowsInstanceOf(() => delete super.prop, ReferenceError);

        return {};
    }
}

new derivedTestDeleteProp();

