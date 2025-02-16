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
function *generatorNewTarget(expected) {
    assert.sameValue(new.target, expected);
    assert.sameValue(eval('new.target'), expected);
    assert.sameValue((() => new.target)(), expected);
    yield (() => new.target);
}

const ITERATIONS = 25;

for (let i = 0; i < ITERATIONS; i++)
    assert.sameValue(generatorNewTarget(undefined).next().value(), undefined);

