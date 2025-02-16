// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var objects = [
    Math.sin.bind(null),
    new Proxy(Math.sin.bind(null), {}),
    Function.prototype.bind.call(new Proxy(Math.sin, {}))
]

for (var obj of objects) {
    // Target is not constructable, so a new array should be created internally.
    assert.deepEqual(Array.from.call(obj, [1, 2, 3]), [1, 2, 3]);
    assert.deepEqual(Array.of.call(obj, 1, 2, 3), [1, 2, 3]);

    // Make sure they are callable, but not constructable.
    obj();
    assertThrowsInstanceOf(() => new obj, TypeError);
}

