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
function C(){}
C.prototype = 1;
try {
    Object.defineProperty(C, "prototype", {get: function() { throw 0; }});
    actual = "no exception";
} catch (exc) {
    actual = exc.name;
}
new C; // don't assert
assert.sameValue(actual, "TypeError");
