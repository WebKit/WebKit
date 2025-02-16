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
var BUGNUMBER = 1287524;
var summary = 'RegExp.prototype[@@replace] should call replacer function after collecting all matches.';

print(BUGNUMBER + ": " + summary);

var rx = RegExp("a", "g");
var r = rx[Symbol.replace]("abba", function() {
    rx.compile("b", "g");
    return "?";
});
assert.sameValue(r, "?bb?");

rx = RegExp("a", "g");
r = "abba".replace(rx, function() {
    rx.compile("b", "g");
    return "?";
});
assert.sameValue(r, "?bb?");

