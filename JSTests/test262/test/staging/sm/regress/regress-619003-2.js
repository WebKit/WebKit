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
var expect = "global";
var actual = expect;
function f([actual]) { }
f(["local"]);
assert.sameValue(expect, actual, "ok");
