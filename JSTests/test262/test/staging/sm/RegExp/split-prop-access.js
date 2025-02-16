// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1287525;
var summary = 'String.prototype.split should call ToUint32(limit) before ToString(separator).';

print(BUGNUMBER + ": " + summary);

var accessed = false;

var rx = /a/;
Object.defineProperty(rx, Symbol.match, {
  get() {
    accessed = true;
  }
});
rx[Symbol.split]("abba");

assert.sameValue(accessed, true);

