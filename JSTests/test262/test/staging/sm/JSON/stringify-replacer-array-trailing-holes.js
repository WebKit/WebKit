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
var gTestfile = "stringify-replacer-array-trailing-holes.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 1217069;
var summary =
  "Better/more correct handling for replacer arrays with trailing holes " +
  "through which inherited elements might appear";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var obj = { 0: "hi", 1: "n-nao", 2: "run away!", 3: "bye" };

var s;

var replacer = [0, /* 1 */, /* 2 */, /* 3 */, ];

assert.sameValue(JSON.stringify(obj, replacer),
         '{"0":"hi"}');

var nobj = new Number(0);
nobj.toString = () => { replacer[1] = 1; return 0; };
replacer[0] = nobj;

assert.sameValue(JSON.stringify(obj, replacer),
         '{"0":"hi","1":"n-nao"}');

delete replacer[1];
replacer[0] = 0;

Object.prototype[0] = 0;
Object.prototype[1] = 1;
Object.prototype[2] = 2;
Object.prototype[3] = 3;

assert.sameValue(JSON.stringify(obj, replacer),
         '{"0":"hi","1":"n-nao","2":"run away!","3":"bye"}');

/******************************************************************************/

print("Tests complete");
