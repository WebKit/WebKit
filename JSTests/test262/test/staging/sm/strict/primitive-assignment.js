// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-strict-shell.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
let primitives = [
    10,
    false,
    "test",
    Symbol()
];

let key = "key";

for (let value of primitives) {
    // Doesn't throw outside strict mode.
    assert.sameValue(value.x = 5, 5);
    assert.sameValue(value[key] = 6, 6);

    assertThrowsInstanceOf(function() { "use strict"; value.x = 5; }, TypeError);
    assertThrowsInstanceOf(function() { "use strict"; value[key] = 6; }, TypeError);

    let target = {};
    assert.sameValue(Reflect.set(target, key, 5, value), false);
    assert.sameValue(key in target, false);
}

