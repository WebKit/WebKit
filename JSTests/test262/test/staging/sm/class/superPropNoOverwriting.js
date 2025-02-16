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
class X {
    constructor() {
        Object.defineProperty(this, "prop1", {
            configurable: true,
            writable: false,
            value: 1
        });

        Object.defineProperty(this, "prop2", {
            configurable: true,
            get: function() { return 15; }
        });

        Object.defineProperty(this, "prop3", {
            configurable: true,
            set: function(a) { }
        });

        Object.defineProperty(this, "prop4", {
            configurable: true,
            get: function() { return 20; },
            set: function(a) { }
        });
    }

    f1() {
        super.prop1 = 2;
    }

    f2() {
        super.prop2 = 3;
    }

    f3() {
        super.prop3 = 4;
    }

    f4() {
        super.prop4 = 5;
    }
}

var x = new X();

assertThrowsInstanceOf(() => x.f1(), TypeError);
assert.sameValue(x.prop1, 1);

assertThrowsInstanceOf(() => x.f2(), TypeError);
assert.sameValue(x.prop2, 15);

assertThrowsInstanceOf(() => x.f3(), TypeError);
assert.sameValue(x.prop3, undefined);

assertThrowsInstanceOf(() => x.f4(), TypeError);
assert.sameValue(x.prop4, 20);

