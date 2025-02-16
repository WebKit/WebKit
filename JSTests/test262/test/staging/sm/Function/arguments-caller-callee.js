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
var gTestfile = 'arguments-caller-callee.js';
var BUGNUMBER = 514563;
var summary = "arguments.caller and arguments.callee are poison pills in ES5, " +
              "later changed in ES2017 to only poison pill arguments.callee.";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// behavior

function expectTypeError(fun)
{
  try
  {
    fun();
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assert.sameValue(e instanceof TypeError, true,
             "expected TypeError calling function" +
             ("name" in fun ? " " + fun.name : "") + ", instead got: " + e);
  }
}

function bar() { "use strict"; return arguments; }
assert.sameValue(bar().caller, undefined); // No error when accessing arguments.caller in ES2017+
expectTypeError(function barCallee() { bar().callee; });

function baz() { return arguments; }
assert.sameValue(baz().callee, baz);


// accessor identity

function strictMode() { "use strict"; return arguments; }
var canonicalTTE = Object.getOwnPropertyDescriptor(strictMode(), "callee").get;

var args = strictMode();

var argsCaller = Object.getOwnPropertyDescriptor(args, "caller");
assert.sameValue(argsCaller, undefined);

var argsCallee = Object.getOwnPropertyDescriptor(args, "callee");
assert.sameValue("get" in argsCallee, true);
assert.sameValue("set" in argsCallee, true);
assert.sameValue(argsCallee.get, canonicalTTE);
assert.sameValue(argsCallee.set, canonicalTTE);


/******************************************************************************/

print("All tests passed!");
