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
var BUGNUMBER = 894026;
var summary = "Implement ES6 octal literals";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var chars = ['o', 'O'];

for (var i = 0; i < 8; i++)
{
  if (i === 8)
  {
    chars.forEach(function(v)
    {
      try
      {
        eval('0' + v + i);
        throw "didn't throw";
      }
      catch (e)
      {
        assert.sameValue(e instanceof SyntaxError, true,
                 "no syntax error evaluating 0" + v + i + ", " +
                 "got " + e);
      }
    });
    continue;
  }

  for (var j = 0; j < 8; j++)
  {
    if (j === 8)
    {
      chars.forEach(function(v)
      {
        try
        {
          eval('0' + v + i + j);
          throw "didn't throw";
        }
        catch (e)
        {
          assert.sameValue(e instanceof SyntaxError, true,
                   "no syntax error evaluating 0" + v + i + j + ", " +
                   "got " + e);
        }
      });
      continue;
    }

    for (var k = 0; k < 8; k++)
    {
      if (k === 8)
      {
        chars.forEach(function(v)
        {
          try
          {
            eval('0' + v + i + j + k);
            throw "didn't throw";
          }
          catch (e)
          {
            assert.sameValue(e instanceof SyntaxError, true,
                     "no syntax error evaluating 0" + v + i + j + k + ", " +
                     "got " + e);
          }
        });
        continue;
      }

      chars.forEach(function(v)
      {
        assert.sameValue(eval('0' + v + i + j + k), i * 64 + j * 8 + k);
      });
    }
  }
}

// Off-by-one check: '/' immediately precedes '0'.
assert.sameValue(0o110/2, 36);
assert.sameValue(0O644/2, 210);

function strict()
{
  "use strict";
  return 0o755;
}
assert.sameValue(strict(), 7 * 64 + 5 * 8 + 5);

/******************************************************************************/

print("Tests complete");
