/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 616294;
var summary =
  "|delete x| inside a function in eval code, where that eval code includes " +
  "|var x| at top level, actually does delete the binding for x";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var f;

function testOuterLet()
{
  return eval("let x; (function() { return delete x; })");
}

f = testOuterLet();

assert.sameValue(f(), false); // can't delete lexical declarations => false


function testOuterForLet()
{
  return eval("for (let x; false; ); (function() { return delete x; })");
}

f = testOuterForLet();

assert.sameValue(f(), true); // not there => true (only non-configurable => false)


function testOuterForInLet()
{
  return eval("for (let x in {}); (function() { return delete x; })");
}

f = testOuterForInLet();

assert.sameValue(f(), true); // configurable, so remove => true
assert.sameValue(f(), true); // not there => true (only non-configurable => false)


function testOuterNestedVarInForLet()
{
  return eval("for (let q = 0; q < 5; q++) { var x; } (function() { return delete x; })");
}

f = testOuterNestedVarInForLet();

assert.sameValue(f(), true); // configurable, so remove => true
assert.sameValue(f(), true); // there => true


function testArgumentShadowLet()
{
  return eval("let x; (function(x) { return delete x; })");
}

f = testArgumentShadowLet();

assert.sameValue(f(), false); // non-configurable argument => false


function testFunctionLocal()
{
  return eval("(function() { let x; return delete x; })");
}

f = testFunctionLocal();

assert.sameValue(f(), false); // defined by function code => not configurable => false


/******************************************************************************/

print("All tests passed!");
