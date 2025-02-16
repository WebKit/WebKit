/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
info: |
  preventExtensions on global
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 600128;
var summary =
  "Properly handle attempted addition of properties to non-extensible objects";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var o = Object.freeze({});
for (var i = 0; i < 10; i++)
  print(o.u = "");

Object.freeze(this);
for (let j = 0; j < 10; j++)
  print(u = "");


/******************************************************************************/

print("All tests passed!");
