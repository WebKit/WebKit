/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'assign-to-callee-name.js';
var BUGNUMBER = 610350;
var summary =
  "Assigning to a function expression's name within that function should " +
  "throw a TypeError in strict mode code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var f = function assignSelfStrict() { "use strict"; assignSelfStrict = 12; };

try
{
  var r = f();
  throw new Error("should have thrown a TypeError, returned " + r);
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "didn't throw a TypeError: " + e);
}

var assignSelf = 42;
var f2 = function assignSelf() { assignSelf = 12; };

f2(); // shouldn't throw, does nothing
assertEq(assignSelf, 42);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");

var successfullyParsed = true;
