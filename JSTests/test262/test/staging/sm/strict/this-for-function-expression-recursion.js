/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-strict-shell.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'this-for-function-expression-recursion.js';
var BUGNUMBER = 611276;
var summary = "JSOP_CALLEE should push undefined, not null, for this";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Calling a named function expression (not function statement) uses the
// JSOP_CALLEE opcode.  This opcode pushes its own |this|, distinct from the
// normal call path; verify that that |this| value is properly |undefined|.

var calleeThisFun =
  function calleeThisFun(recurring)
  {
    if (recurring)
      return this;
    return calleeThisFun(true);
  };
assert.sameValue(calleeThisFun(false), this);

var calleeThisStrictFun =
  function calleeThisStrictFun(recurring)
  {
    "use strict";
    if (recurring)
      return this;
    return calleeThisStrictFun(true);
  };
assert.sameValue(calleeThisStrictFun(false), undefined);

/******************************************************************************/

print("All tests passed!");
