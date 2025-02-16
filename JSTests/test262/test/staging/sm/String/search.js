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
var summary = "Call RegExp.prototype[@@search] from String.prototype.search.";

print(BUGNUMBER + ": " + summary);

var called = 0;
var myRegExp = {
  [Symbol.search](S) {
    assert.sameValue(S, "abcAbcABC");
    called++;
    return 42;
  }
};
assert.sameValue("abcAbcABC".search(myRegExp), 42);
assert.sameValue(called, 1);

called = 0;
RegExp.prototype[Symbol.search] = function(S) {
  assert.sameValue(S, "abcAbcABC");
  called++;
  return 43;
};
assert.sameValue("abcAbcABC".search("abc"), 43);
assert.sameValue(called, 1);

