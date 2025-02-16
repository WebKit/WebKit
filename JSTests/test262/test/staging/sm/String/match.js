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
var BUGNUMBER = 887016;
var summary = "Call RegExp.prototype[@@match] from String.prototype.match.";

print(BUGNUMBER + ": " + summary);

var called = 0;
var myRegExp = {
  [Symbol.match](S) {
    assert.sameValue(S, "abcAbcABC");
    called++;
    return 42;
  }
};
assert.sameValue("abcAbcABC".match(myRegExp), 42);
assert.sameValue(called, 1);

var origMatch = RegExp.prototype[Symbol.match];

called = 0;
RegExp.prototype[Symbol.match] = function(S) {
  assert.sameValue(S, "abcAbcABC");
  called++;
  return 43;
};
assert.sameValue("abcAbcABC".match("abc"), 43);
assert.sameValue(called, 1);

RegExp.prototype[Symbol.match] = origMatch;

