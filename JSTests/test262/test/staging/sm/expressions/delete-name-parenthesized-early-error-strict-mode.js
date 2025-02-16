/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 1111101;
var summary =
  "delete (foo), delete ((foo)), and so on are strict mode early errors";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function checkSyntaxError(code)
{
  function helper(maker)
  {
    var earlyError = false;
    try
    {
      var f = maker(code);

      var error = "no early error, created a function with code <" + code + ">";
      try
      {
        f();
        error += ", and the function can be called without error";
      }
      catch (e)
      {
        error +=", and calling the function throws " + e;
      }

      throw new Error(error);
    }
    catch (e)
    {
      assert.sameValue(e instanceof SyntaxError, true,
               "expected syntax error, got " + e);
    }
  }

  helper(Function);
  helper(eval);
}

checkSyntaxError("function f() { 'use strict'; delete escape; } f();");
checkSyntaxError("function f() { 'use strict'; delete escape; }");
checkSyntaxError("function f() { 'use strict'; delete (escape); } f();");
checkSyntaxError("function f() { 'use strict'; delete (escape); }");
checkSyntaxError("function f() { 'use strict'; delete ((escape)); } f();");
checkSyntaxError("function f() { 'use strict'; delete ((escape)); }");

// Meanwhile, non-strict all of these should work

function checkFine(code)
{
  Function(code);
  (1, eval)(code); // indirect, to be consistent w/above
}

checkFine("function f() { delete escape; } f();");
checkFine("function f() { delete escape; }");
checkFine("function f() { delete (escape); } f();");
checkFine("function f() { delete (escape); }");
checkFine("function f() { delete ((escape)); } f();");
checkFine("function f() { delete ((escape)); }");


/******************************************************************************/

print("Tests complete");
