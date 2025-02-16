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
function boundTarget(expected) {
    assert.sameValue(new.target, expected);
}

let bound = boundTarget.bind(undefined);

const TEST_ITERATIONS = 550;

for (let i = 0; i < TEST_ITERATIONS; i++)
    bound(undefined);

for (let i = 0; i < TEST_ITERATIONS; i++)
    new bound(boundTarget);

