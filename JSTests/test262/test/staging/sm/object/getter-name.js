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
var BUGNUMBER = 1180290;
var summary = 'Object accessors should have get prefix';

print(BUGNUMBER + ": " + summary);

assert.sameValue(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get.name, "get __proto__");
assert.sameValue(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").set.name, "set __proto__");

