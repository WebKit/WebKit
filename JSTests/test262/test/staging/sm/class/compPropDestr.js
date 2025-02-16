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
var BUGNUMBER = 924688;
var summary = 'Computed Property Names';

print(BUGNUMBER + ": " + summary);

var key = "z";
var { [key]: foo } = { z: "bar" };
assert.sameValue(foo, "bar");

