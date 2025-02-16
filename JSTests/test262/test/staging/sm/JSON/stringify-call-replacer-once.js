/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-JSON-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'stringify-call-replacer-once.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary = "Call replacer function exactly once per value";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var factor = 1;
function replacer(k, v)
{
  if (k === "")
    return v;

  return v * ++factor;
}

var obj = { a: 1, b: 2, c: 3 };

assert.sameValue(JSON.stringify(obj, replacer), '{"a":2,"b":6,"c":12}');
assert.sameValue(factor, 4);

/******************************************************************************/

print("Tests complete");
