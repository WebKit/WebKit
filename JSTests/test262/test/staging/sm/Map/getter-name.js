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
var BUGNUMBER = 1180290;
var summary = 'Map getters should have get prefix';

print(BUGNUMBER + ": " + summary);

assert.sameValue(Object.getOwnPropertyDescriptor(Map, Symbol.species).get.name, "get [Symbol.species]");
assert.sameValue(Object.getOwnPropertyDescriptor(Map.prototype, "size").get.name, "get size");

