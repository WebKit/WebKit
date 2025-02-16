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
var BUGNUMBER = 1269719;
var summary = "RegExp.prototype[@@replace] should check latin1/twoBytes for all strings used in relate operation.";

print(BUGNUMBER + ": " + summary);

var ans = [
  "[AB$2$3$]",
  "[AB$2$3$]\u3048",
  "[AB$2$3$]",
  "[AB$2$3$]\u3048",
  "[A\u3044$2$3$]",
  "[A\u3044$2$3$]\u3048",
  "[A\u3044$2$3$]",
  "[A\u3044$2$3$]\u3048",
  "[\u3042B$2$3$]",
  "[\u3042B$2$3$]\u3048",
  "[\u3042B$2$3$]",
  "[\u3042B$2$3$]\u3048",
  "[\u3042\u3044$2$3$]",
  "[\u3042\u3044$2$3$]\u3048",
  "[\u3042\u3044$2$3$]",
  "[\u3042\u3044$2$3$]\u3048",
];
var i = 0;
for (var matched of ["A", "\u3042"]) {
  for (var group1 of ["B", "\u3044"]) {
    for (var string of ["C", "\u3046"]) {
      for (var replacement of ["[$&$`$'$1$2$3$]", "[$&$`$'$1$2$3$]\u3048"]) {
        var myRegExp = {
          get exec() {
            return function() {
              return [matched, group1];
            };
          }
        };
        assert.sameValue(RegExp.prototype[Symbol.replace].call(myRegExp, string, replacement), ans[i]);
        i++;
      }
    }
  }
}

