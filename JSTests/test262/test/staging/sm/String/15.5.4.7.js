/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 612838;
var summary = "String.prototype.indexOf with empty searchString";

print(BUGNUMBER + ": " + summary);

assert.sameValue("123".indexOf("", -1), 0);
assert.sameValue("123".indexOf("", 0), 0);
assert.sameValue("123".indexOf("", 1), 1);
assert.sameValue("123".indexOf("", 3), 3);
assert.sameValue("123".indexOf("", 4), 3);
assert.sameValue("123".indexOf("", 12345), 3);

print("All tests passed!");
