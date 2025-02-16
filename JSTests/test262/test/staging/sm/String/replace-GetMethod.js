// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1290655;
var summary = "String.prototype.replace should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.replace]: value,
        toString() {
            return "-";
        }
    };
}

for (let v of [null, undefined]) {
    assert.sameValue("a-a".replace(create(v), "+"), "a+a");
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".replace(create(v)), TypeError);
}

