/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 805121;
var summary = "Be more careful with string math to avoid wrong results";

print(BUGNUMBER + ": " + summary);

/******************************************************************************/

function puff(x, n)
{
  while(x.length < n)
    x += x;
  return x.substring(0, n);
}

var x = puff("1", 1 << 20);
var rep = puff("$1", 1 << 16);

try
{
  var y = x.replace(/(.+)/g, rep);
  assert.sameValue(y.length, Math.pow(2, 36));
}
catch (e)
{
  // OOM also acceptable
}

/******************************************************************************/

print("Tests complete");
