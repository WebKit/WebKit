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
var BUGNUMBER = 1288459;
var summary = "|yield| is sometimes a valid identifier";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function t(code)
{
  var strictSemi = " 'use strict'; " + code;
  var strictASI = " 'use strict' \n " + code;

  var creationFunctions = ["Function"];
  if (typeof evaluate === "function")
    creationFunctions.push("evaluate");
  if (typeof parseModule === "function")
    creationFunctions.push("parseModule");

  for (var func of creationFunctions)
  {
    var g = createNewGlobal();
    var f = g[func];

    if (func === "parseModule")
      assertThrowsInstanceOf(() => f(code), g.SyntaxError);
    else
      f(code);

    assertThrowsInstanceOf(() => f(strictSemi), g.SyntaxError);
    assertThrowsInstanceOf(() => f(strictASI), g.SyntaxError);
  }
}

t("var yield = 3;");
t("let yield = 3;");
t("const yield = 3;");
t("for (var yield = 3; ; ) break;");
t("for (let yield = 3; ; ) break;");
t("for (const yield = 3; ; ) break;");

/******************************************************************************/

print("Tests complete");
