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
foo = 1;
Object.defineProperty(this, "foo", {writable:false, configurable:true});
foo = 2;
assert.sameValue(foo, 1);

