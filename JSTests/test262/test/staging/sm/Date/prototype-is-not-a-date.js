/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Date-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 861219;
var summary = "Date.prototype isn't an instance of Date";

print(BUGNUMBER + ": " + summary);

assert.sameValue(Date.prototype instanceof Date, false);
assert.sameValue(Date.prototype.__proto__, Object.prototype);

