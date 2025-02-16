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
var summary = "String.prototype.match should call GetMethod.";

print(BUGNUMBER + ": " + summary);

function create(value) {
    return {
        [Symbol.match]: value,
        toString() {
            return "-";
        }
    };
}

var expected = ["-"];
expected.index = 1;
expected.input = "a-a";
expected.groups = undefined;

for (let v of [null, undefined]) {
    assert.deepEqual("a-a".match(create(v)), expected);
}

for (let v of [1, true, Symbol.iterator, "", {}, []]) {
    assertThrowsInstanceOf(() => "a-a".match(create(v)), TypeError);
}

