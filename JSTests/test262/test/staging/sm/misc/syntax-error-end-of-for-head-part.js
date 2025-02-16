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
//-----------------------------------------------------------------------------
var BUGNUMBER = 672854;
var summary =
  "Syntax errors at the end of |for| statement header parts shouldn't cause " +
  "crashes";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function checkSyntaxError(str)
{
  try
  {
    var f = Function("for(w in\\");
    throw new Error("didn't throw, returned " + f);
  }
  catch (e)
  {
    assert.sameValue(e instanceof SyntaxError, true,
             "expected SyntaxError, got " + e);
  }
}

checkSyntaxError("for(var w in \\");
checkSyntaxError("for(w in \\");
checkSyntaxError("for(var w\\");
checkSyntaxError("for(w\\");
checkSyntaxError("for(var w;\\");
checkSyntaxError("for(w;\\");
checkSyntaxError("for(var w; w >\\");
checkSyntaxError("for(w; w >\\");
checkSyntaxError("for(var w; w > 3;\\");
checkSyntaxError("for(w; w > 3;\\");
checkSyntaxError("for(var w; w > 3; 5\\");
checkSyntaxError("for(w; w > 3; 5\\");
checkSyntaxError("for(var w; w > 3; 5foo");
checkSyntaxError("for(w; w > 3; 5foo");

/******************************************************************************/

print("Tests complete!");
