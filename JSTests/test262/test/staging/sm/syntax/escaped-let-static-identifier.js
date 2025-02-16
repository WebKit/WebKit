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
var BUGNUMBER = 1288460;
var summary =
  "|let| and |static| are forbidden as Identifier only in strict mode code, " +
  "and it's permissible to use them as Identifier (with or without " +
  "containing escapes) in non-strict code";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function t(code)
{
  var strictSemi = " 'use strict'; " + code;
  var strictASI = " 'use strict' \n " + code;

  var creationFunctions = [Function];
  if (typeof evaluate === "function")
    creationFunctions.push(evaluate);
  if (typeof parseModule === "function")
    creationFunctions.push(parseModule);

  for (var func of creationFunctions)
  {
    if (typeof parseModule === "function" && func === parseModule)
      assertThrowsInstanceOf(() => func(code), SyntaxError);
    else
      func(code);

    assertThrowsInstanceOf(() => func(strictSemi), SyntaxError);
    assertThrowsInstanceOf(() => func(strictASI), SyntaxError);
  }
}

t("l\\u0065t: 42;");
t("if (1) l\\u0065t: 42;");
t("l\\u0065t = 42;");
t("if (1) l\\u0065t = 42;");

t("st\\u0061tic: 42;");
t("if (1) st\\u0061tic: 42;");
t("st\\u0061tic = 42;");
t("if (1) st\\u0061tic = 42;");

/******************************************************************************/

print("Tests complete");
