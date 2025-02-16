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
var summary = "RegExpExec should throw if exec property of non-RegExp is not callable";

print(BUGNUMBER + ": " + summary);

for (var exec of [null, 0, false, undefined, ""]) {
  // RegExp with non-callable exec
  var re = /a/;
  re.exec = exec;
  RegExp.prototype[Symbol.match].call(re, "foo");

  // non-RegExp with non-callable exec
  assertThrowsInstanceOf(() => RegExp.prototype[Symbol.match].call({ exec }, "foo"),
                         TypeError);
}

