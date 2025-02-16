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
//-----------------------------------------------------------------------------
var gTestfile = "for-in-with-declaration.js";
var BUGNUMBER = 1163851;
var summary =
  "Declarations in for-in loop heads must not contain |in|-expression " +
  "initializers";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertThrowsInstanceOf(() => Function("for (var x = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (var x, y = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (var x = 5, y = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (const x = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (const x = 5, y = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (let x = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (let x, y = 3 in {}; ; ) break;"), SyntaxError);
assertThrowsInstanceOf(() => Function("for (let x = 2, y = 3 in {}; ; ) break;"), SyntaxError);

/******************************************************************************/

print("Tests complete");
