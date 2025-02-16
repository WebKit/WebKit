// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Optional expression can be part of a class heritage expression.

var a = {b: null};

class C extends a?.b {}

assert.sameValue(Object.getPrototypeOf(C.prototype), null);

