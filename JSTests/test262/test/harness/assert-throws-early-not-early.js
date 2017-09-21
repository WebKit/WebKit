// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    The assertion fails when the code does not parse with an early error
includes: [sta.js]
---*/
assert.throws(Test262Error, () => {
  assert.throws.early(ReferenceError, 'x = 1');
});
