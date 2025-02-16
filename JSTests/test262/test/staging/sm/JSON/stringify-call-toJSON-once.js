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
var gTestfile = 'stringify-call-toJSON-once.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 584909;
var summary = "Stringification of Boolean/String/Number objects";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj =
  {
    p: {
         toJSON: function()
         {
           return { toJSON: function() { return 17; } };
         }
       }
  };

assert.sameValue(JSON.stringify(obj), '{"p":{}}');

/******************************************************************************/

print("Tests complete");
