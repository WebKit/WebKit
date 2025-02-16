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
var summary = "Implement ES6 binary literals";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var chars = ['b', 'B'];

for (var i = 0; i < 2; i++)
{
  if (i === 2)
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

  for (var j = 0; j < 2; j++)
  {
    if (j === 2)
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

    for (var k = 0; k < 2; k++)
    {
      if (k === 2)
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
        assert.sameValue(eval('0' + v + i + j + k), i * 4 + j * 2 + k);
      });
    }
  }
}

chars.forEach(function(v)
{
  try
  {
  }
  catch (e)
  {
    assert.sameValue(e instanceof SyntaxError, true,
             "no syntax error evaluating 0" + v + ", got " + e);
  }
});

// Off-by-one check: '/' immediately precedes '0'.
assert.sameValue(0b110/1, 6);
assert.sameValue(0B10110/1, 22);

function strict()
{
  "use strict";
  return 0b11010101;
}
assert.sameValue(strict(), 128 + 64 + 16 + 4 + 1);

/******************************************************************************/

print("Tests complete");
