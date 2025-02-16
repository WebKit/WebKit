/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 622167;
var summary = 'Handle infinite recursion';
print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function eval() { eval(); }

function DoWhile_3()
{
  eval();
}

try
{
  DoWhile_3();
}
catch(e) { }

var r;
function* f()
{
  r = arguments;
  test();
  yield 170;
}

function test()
{
  function foopy()
  {
    try
    {
      for (var i of f());
    }
    catch (e)
    {
      $262.gc();
    }
  }
  foopy();
}
test();

/******************************************************************************/

print("All tests passed!");
