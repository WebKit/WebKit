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
var BUGNUMBER = 887016;
var summary = "Implement RegExp.prototype[@@split].";

print(BUGNUMBER + ": " + summary);

assert.sameValue(RegExp.prototype[Symbol.split].name, "[Symbol.split]");
assert.sameValue(RegExp.prototype[Symbol.split].length, 2);
var desc = Object.getOwnPropertyDescriptor(RegExp.prototype, Symbol.split);
assert.sameValue(desc.configurable, true);
assert.sameValue(desc.enumerable, false);
assert.sameValue(desc.writable, true);

var re = /b/;
var v = re[Symbol.split]("abcAbcABC");
assert.sameValue(JSON.stringify(v), `["a","cA","cABC"]`);

re = /d/;
v = re[Symbol.split]("abcAbcABC");
assert.sameValue(JSON.stringify(v), `["abcAbcABC"]`);

re = /b/ig;
v = re[Symbol.split]("abcAbcABC");
assert.sameValue(JSON.stringify(v), `["a","cA","cA","C"]`);

re = /b/ig;
v = re[Symbol.split]("abcAbcABC", 2);
assert.sameValue(JSON.stringify(v), `["a","cA"]`);

