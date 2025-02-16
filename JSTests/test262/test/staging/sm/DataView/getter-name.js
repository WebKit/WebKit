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
var summary = 'DataView getters should have get prefix';

print(BUGNUMBER + ": " + summary);

assert.sameValue(Object.getOwnPropertyDescriptor(DataView.prototype, "buffer").get.name, "get buffer");
assert.sameValue(Object.getOwnPropertyDescriptor(DataView.prototype, "byteLength").get.name, "get byteLength");
assert.sameValue(Object.getOwnPropertyDescriptor(DataView.prototype, "byteOffset").get.name, "get byteOffset");

