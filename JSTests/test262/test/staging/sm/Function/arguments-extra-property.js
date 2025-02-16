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
var BUGNUMBER = 1263811;
var summary = "GetElem for modified arguments shouldn't be optimized to get original argument.";

print(BUGNUMBER + ": " + summary);

function testModifyFirst() {
    function f() {
        Object.defineProperty(arguments, 1, { value: 30 });
        assert.sameValue(arguments[1], 30);
    }
    for (let i = 0; i < 10; i++)
        f(10, 20);
}
testModifyFirst();

function testModifyLater() {
    function f() {
        var ans = 20;
        for (let i = 0; i < 10; i++) {
            if (i == 5) {
                Object.defineProperty(arguments, 1, { value: 30 });
                ans = 30;
            }
            assert.sameValue(arguments[1], ans);
        }
    }
    for (let i = 0; i < 10; i++)
        f(10, 20);
}
testModifyLater();

