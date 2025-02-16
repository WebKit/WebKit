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
var gTestfile = "for-loop-with-bindings-added-at-runtime.js";
//-----------------------------------------------------------------------------
var BUGNUMBER = 1149797;
var summary =
  "Don't assert when freshening the scope chain for a for-loop whose head " +
  "contains a lexical declaration, where the loop body might add more " +
  "bindings at runtime";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

for (let x = 0; x < 9; ++x)
  eval("var y");

{
  for (let x = 0; x < 9; ++x)
    eval("var y");
}

function f1()
{
  for (let x = 0; x < 9; ++x)
    eval("var y");
}
f1();

function f2()
{
  {
    for (let x = 0; x < 9; ++x)
      eval("var y");
  }
}
f2();

for (let x = 0; x < 9; ++x)
{
  // deliberately inside a block statement
  eval("var y");
}

{
  for (let x = 0; x < 9; ++x)
  {
    // deliberately inside a block statement
    eval("var y");
  }
}

function g1()
{
  for (let x = 0; x < 9; ++x)
  {
    // deliberately inside a block statement
    eval("var y");
  }
}
g1();

function g2()
{
  {
    for (let x = 0; x < 9; ++x)
    {
      // deliberately inside a block statement
      eval("var y");
    }
  }
}
g2();

for (let x = 0; x < 9; ++x) {
  (function() {
      eval("var y");
  })();
}

{
  for (let x = 0; x < 9; ++x)
  {
    // deliberately inside a block statement
    (function() {
        eval("var y");
    })();
  }
}

function h1()
{
  for (let x = 0; x < 9; ++x)
  {
    // deliberately inside a block statement
    (function() {
        eval("var y");
    })();
  }
}
h1();

function h2()
{
  {
    for (let x = 0; x < 9; ++x)
    {
      // deliberately inside a block statement
      (function() { eval("var y"); })();
    }
  }
}
h2();

/******************************************************************************/

print("Tests complete");
