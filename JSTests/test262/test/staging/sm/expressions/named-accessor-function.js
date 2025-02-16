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
// Contributor:
//   Jeff Walden <jwalden+code@mit.edu>

//-----------------------------------------------------------------------------
var BUGNUMBER = 999999;
var summary = '{ get x y() { } } is not valid getter syntax';

print(BUGNUMBER + ": " + summary);

var BAD_CODE = ["({ get x y() { } })", "({ set x y(v) { } })"];

for (var i = 0, sz = BAD_CODE.length; i < sz; i++)
{
  var code = BAD_CODE[i];

  var err = "no exception";
  try
  {
    eval(code);
  }
  catch (e)
  {
    err = e;
  }
  if (!(err instanceof SyntaxError))
  {
    assert.sameValue(true, false,
             "bad or no exception thrown for eval(" + code + "): " + err);
  }

  err = "no exception";
  try
  {
    new Function(code);
  }
  catch (e)
  {
    err = e;
  }
  if (!(err instanceof SyntaxError))
  {
    assert.sameValue(true, false,
             "bad or no exception thrown for Function(" + code + "): " + err);
  }
}

