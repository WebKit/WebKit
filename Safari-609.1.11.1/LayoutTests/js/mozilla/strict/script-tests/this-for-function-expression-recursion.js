/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

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
assertEq(calleeThisFun(false), this);

var calleeThisStrictFun =
  function calleeThisStrictFun(recurring)
  {
    "use strict";
    if (recurring)
      return this;
    return calleeThisStrictFun(true);
  };
assertEq(calleeThisStrictFun(false), undefined);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

var successfullyParsed = true;
