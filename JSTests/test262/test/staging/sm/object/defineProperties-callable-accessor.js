// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// ObjectDefineProperties with non callable accessor throws.
const descriptors = [
    {get: 1}, {set: 1},
    {get: []}, {set: []},
    {get: {}}, {set: {}},
    {get: new Number}, {set: new Number},

    {get: 1, set: 1},
    {get: [], set: []},
    {get: {}, set: {}},
    {get: new Number, set: new Number},
];

for (const descriptor of descriptors) {
    assertThrowsInstanceOf(() => Object.create(null, {x: descriptor}), TypeError);
    assertThrowsInstanceOf(() => Object.defineProperties({}, {x: descriptor}), TypeError);
}

