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
var gTestfile = 'stringify-toJSON-arguments.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary = "Arguments when an object's toJSON method is called";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj =
  {
    p: {
         toJSON: function(key)
         {
           assert.sameValue(arguments.length, 1);
           assert.sameValue(key, "p");
           return 17;
         }
       }
  };

assert.sameValue(JSON.stringify(obj), '{"p":17}');

/******************************************************************************/

print("Tests complete");
