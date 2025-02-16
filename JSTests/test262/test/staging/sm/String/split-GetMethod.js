// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1290655;
var summary = "String.prototype.split should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.split]: value,
        toString() {
            return "-";
        }
    };
}

for (let v of [null, undefined]) {
    assert.deepEqual("a-a".split(create(v)), ["a", "a"]);
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".split(create(v)), TypeError);
}

