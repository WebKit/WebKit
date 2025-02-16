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
var BUGNUMBER = 614608;
var summary = "String.prototype.split with undefined separator";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function assertEqArr(a1, a2) {
    assert.sameValue(a1.length, a2.length);

    for(var i=0; i<a1.length; i++) {
        assert.sameValue(a1[i], a2[i]);
    }
}
var s = '--undefined--undefined--';

assertEqArr(s.split(undefined, undefined), [s]);
assertEqArr(s.split(undefined, -1), [s]);

assertEqArr(s.split(undefined, 1), [s]);
assertEqArr(s.split("undefined", 1), ["--"]);

assertEqArr(s.split("-", 0), []);
assertEqArr(s.split(undefined, 0), []);
assertEqArr(s.split(s, 0), []);

print("All tests passed!");
