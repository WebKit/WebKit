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
function f(x) { return eval('"mumble"; x + 42'); }

var expect = true;
var actual = ('' + f).indexOf("mumble") >= 0;

assert.sameValue(expect, actual, "unknown directive in eval code wrongly dropped");
