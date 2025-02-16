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
var x = 0;
var y = 1;
var g = 1;

var expect = "y";
var actual;

try {
    eval("y: while (x) break\n/y/g.exec('y')");
    actual = RegExp.lastMatch;
} catch (e) {
    actual = '' + e;
}
assert.sameValue(actual, expect);

try {
    eval("y: while (x) continue\n/y/g.exec('y')");
    actual = RegExp.lastMatch;
} catch (e) {
    actual = '' + e;
}
assert.sameValue(actual, expect);

