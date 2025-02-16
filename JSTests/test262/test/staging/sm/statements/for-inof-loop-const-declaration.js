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
var gTestfile = "for-inof-loop-const-declaration.js";
var BUGNUMBER = 1278150;
var summary = "Support const declarations in for-of loop heads";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var count;

count = 0;
for (const x in "abcdef")
{
  assert.sameValue(x, "" + count++);

  try
  {
    x = 3;
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "didn't get a TypeError, instead got: " + e);
  }
}

count = 0;
for (const x of "012345")
{
  assert.sameValue(x, "" + count++);

  try
  {
    x = 3;
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "didn't get a TypeError, instead got: " + e);
  }
}

count = 0;
for (const { length, 0: c } in "abcdef")
{
  assert.sameValue(length, 1);
  assert.sameValue(c, "" + count++);


  try
  {
    length = 1;
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "didn't get a TypeError, instead got: " + e);
  }
}

count = 0;
for (const { length, 0: c } of "012345")
{
  assert.sameValue(length, 1);
  assert.sameValue(c, "" + count++);

  try
  {
    c = 42;
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "didn't get a TypeError, instead got: " + e);
  }
}

/******************************************************************************/

print("Tests complete");
