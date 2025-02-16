// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1054755;
var summary = 'String.prototype.{startsWith,endsWith,includes} should call IsRegExp.';

print(BUGNUMBER + ": " + summary);

for (var method of ["startsWith", "endsWith", "includes"]) {
  for (var re of [/foo/, new RegExp()]) {
    assertThrowsInstanceOf(() => "foo"[method](re), TypeError);

    re[Symbol.match] = false;
    "foo"[method](re);
  }

  for (var v1 of [true, 1, "bar", [], {}, Symbol.iterator]) {
    assertThrowsInstanceOf(() => "foo"[method]({ [Symbol.match]: v1 }), TypeError);
  }

  for (var v2 of [false, 0, undefined, ""]) {
    "foo"[method]({ [Symbol.match]: v2 });
  }
}

