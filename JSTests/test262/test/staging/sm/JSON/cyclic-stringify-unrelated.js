/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 1197097;
var summary = "JSON.stringify shouldn't use context-wide cycle detection";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var arr;

// Nested yet separate JSON.stringify is okay.
arr = [{}];
assert.sameValue(JSON.stringify(arr, function(k, v) {
  assert.sameValue(JSON.stringify(arr), "[{}]");
  return v;
}), "[{}]");

// SpiderMonkey censors cycles in array-joining.  This mechanism must not
// interfere with the cycle detection in JSON.stringify.
arr = [{
  toString: function() {
    var s = JSON.stringify(arr);
    assert.sameValue(s, "[{}]");
    return s;
  }
}];
assert.sameValue(arr.join(), "[{}]");

/******************************************************************************/

print("Tests complete");
