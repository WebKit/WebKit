/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function f(x) { return 1 + "" + (x + 1); }
assert.sameValue("12", f(1), "");
var g = eval("(" + f + ")");
assert.sameValue("12", g(1), "");
