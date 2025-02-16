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
var summary = "RegExpExec should throw if returned value is not an object nor null.";

print(BUGNUMBER + ": " + summary);

for (var ret of [null, {}, [], /a/]) {
  assert.sameValue(RegExp.prototype[Symbol.match].call({
    get global() {
      return false;
    },
    exec(S) {
      return ret;
    }
  }, "foo"), ret);
}

for (ret of [undefined, 1, true, false, Symbol.iterator]) {
  assertThrowsInstanceOf(() => {
    RegExp.prototype[Symbol.match].call({
      get global() {
        return false;
      },
      exec(S) {
        return ret;
      }
    }, "foo");
  }, TypeError);
}

