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
// Make sure that we can plumb new.target, even if the results are going to
// throw.

assertThrowsInstanceOf(() => new ""(...Array()), TypeError);

assertThrowsInstanceOf(() => new ""(), TypeError);
assertThrowsInstanceOf(() => new ""(1), TypeError);

