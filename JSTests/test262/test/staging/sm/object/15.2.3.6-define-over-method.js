/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 568786;
var summary =
  'Do not assert: !(attrs & (JSPROP_GETTER | JSPROP_SETTER)) with ' +
  'Object.defineProperty';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = { x: function(){} };
Object.defineProperty(o, "x", { get: function(){} });

/******************************************************************************/

print("All tests passed!");
