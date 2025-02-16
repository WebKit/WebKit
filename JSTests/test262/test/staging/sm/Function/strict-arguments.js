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
var gTestfile = 'strict-arguments.js';
var BUGNUMBER = 516255;
var summary =
  "ES5 strict mode: arguments objects of strict mode functions must copy " +
  "argument values";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function arrayEvery(arr, fun)
{
  return Array.prototype.every.call(arr, fun);
}

function arraysEqual(a1, a2)
{
  return a1.length === a2.length &&
         arrayEvery(a1, function(v, i) { return v === a2[i]; });
}


/************************
 * NON-STRICT ARGUMENTS *
 ************************/

var obj = {};

function noargs() { return arguments; }

assert.sameValue(arraysEqual(noargs(), []), true);
assert.sameValue(arraysEqual(noargs(1), [1]), true);
assert.sameValue(arraysEqual(noargs(2, obj, 8), [2, obj, 8]), true);

function args(a) { return arguments; }

assert.sameValue(arraysEqual(args(), []), true);
assert.sameValue(arraysEqual(args(1), [1]), true);
assert.sameValue(arraysEqual(args(1, obj), [1, obj]), true);
assert.sameValue(arraysEqual(args("foopy"), ["foopy"]), true);

function assign(a)
{
  a = 17;
  return arguments;
}

assert.sameValue(arraysEqual(assign(1), [17]), true);

function getLaterAssign(a)
{
  var o = arguments;
  a = 17;
  return o;
}

assert.sameValue(arraysEqual(getLaterAssign(1), [17]), true);

function assignElementGetParameter(a)
{
  arguments[0] = 17;
  return a;
}

assert.sameValue(assignElementGetParameter(42), 17);

function assignParameterGetElement(a)
{
  a = 17;
  return arguments[0];
}

assert.sameValue(assignParameterGetElement(42), 17);

function assignArgSub(x, y)
{
  arguments[0] = 3;
  return arguments[0];
}

assert.sameValue(assignArgSub(1), 3);

function assignArgSubParamUse(x, y)
{
  arguments[0] = 3;
  assert.sameValue(x, 3);
  return arguments[0];
}

assert.sameValue(assignArgSubParamUse(1), 3);

function assignArgumentsElement(x, y)
{
  arguments[0] = 3;
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assert.sameValue(assignArgumentsElement(1), 3);

function assignArgumentsElementParamUse(x, y)
{
  arguments[0] = 3;
  assert.sameValue(x, 3);
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assert.sameValue(assignArgumentsElementParamUse(1), 3);

/********************
 * STRICT ARGUMENTS *
 ********************/

function strictNoargs()
{
  "use strict";
  return arguments;
}

assert.sameValue(arraysEqual(strictNoargs(), []), true);
assert.sameValue(arraysEqual(strictNoargs(1), [1]), true);
assert.sameValue(arraysEqual(strictNoargs(1, obj), [1, obj]), true);

function strictArgs(a)
{
  "use strict";
  return arguments;
}

assert.sameValue(arraysEqual(strictArgs(), []), true);
assert.sameValue(arraysEqual(strictArgs(1), [1]), true);
assert.sameValue(arraysEqual(strictArgs(1, obj), [1, obj]), true);

function strictAssign(a)
{
  "use strict";
  a = 17;
  return arguments;
}

assert.sameValue(arraysEqual(strictAssign(), []), true);
assert.sameValue(arraysEqual(strictAssign(1), [1]), true);
assert.sameValue(arraysEqual(strictAssign(1, obj), [1, obj]), true);

var upper;
function strictAssignAfter(a)
{
  "use strict";
  upper = arguments;
  a = 42;
  return upper;
}

assert.sameValue(arraysEqual(strictAssignAfter(), []), true);
assert.sameValue(arraysEqual(strictAssignAfter(17), [17]), true);
assert.sameValue(arraysEqual(strictAssignAfter(obj), [obj]), true);

function strictMaybeAssignOuterParam(p)
{
  "use strict";
  function inner() { p = 17; }
  return arguments;
}

assert.sameValue(arraysEqual(strictMaybeAssignOuterParam(), []), true);
assert.sameValue(arraysEqual(strictMaybeAssignOuterParam(42), [42]), true);
assert.sameValue(arraysEqual(strictMaybeAssignOuterParam(obj), [obj]), true);

function strictAssignOuterParam(p)
{
  "use strict";
  function inner() { p = 17; }
  inner();
  return arguments;
}

assert.sameValue(arraysEqual(strictAssignOuterParam(), []), true);
assert.sameValue(arraysEqual(strictAssignOuterParam(17), [17]), true);
assert.sameValue(arraysEqual(strictAssignOuterParam(obj), [obj]), true);

function strictAssignOuterParamPSYCH(p)
{
  "use strict";
  function inner(p) { p = 17; }
  inner();
  return arguments;
}

assert.sameValue(arraysEqual(strictAssignOuterParamPSYCH(), []), true);
assert.sameValue(arraysEqual(strictAssignOuterParamPSYCH(17), [17]), true);
assert.sameValue(arraysEqual(strictAssignOuterParamPSYCH(obj), [obj]), true);

function strictEval(code, p)
{
  "use strict";
  eval(code);
  return arguments;
}

assert.sameValue(arraysEqual(strictEval("1", 2), ["1", 2]), true);
assert.sameValue(arraysEqual(strictEval("arguments"), ["arguments"]), true);
assert.sameValue(arraysEqual(strictEval("p = 2"), ["p = 2"]), true);
assert.sameValue(arraysEqual(strictEval("p = 2", 17), ["p = 2", 17]), true);
assert.sameValue(arraysEqual(strictEval("arguments[0] = 17"), [17]), true);
assert.sameValue(arraysEqual(strictEval("arguments[0] = 17", 42), [17, 42]), true);

function strictMaybeNestedEval(code, p)
{
  "use strict";
  function inner() { eval(code); }
  return arguments;
}

assert.sameValue(arraysEqual(strictMaybeNestedEval("1", 2), ["1", 2]), true);
assert.sameValue(arraysEqual(strictMaybeNestedEval("arguments"), ["arguments"]), true);
assert.sameValue(arraysEqual(strictMaybeNestedEval("p = 2"), ["p = 2"]), true);
assert.sameValue(arraysEqual(strictMaybeNestedEval("p = 2", 17), ["p = 2", 17]), true);

function strictNestedEval(code, p)
{
  "use strict";
  function inner() { eval(code); }
  inner();
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedEval("1", 2), ["1", 2]), true);
assert.sameValue(arraysEqual(strictNestedEval("arguments"), ["arguments"]), true);
assert.sameValue(arraysEqual(strictNestedEval("p = 2"), ["p = 2"]), true);
assert.sameValue(arraysEqual(strictNestedEval("p = 2", 17), ["p = 2", 17]), true);
assert.sameValue(arraysEqual(strictNestedEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assert.sameValue(arraysEqual(strictNestedEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictAssignArguments(a)
{
  "use strict";
  arguments[0] = 42;
  return a;
}

assert.sameValue(strictAssignArguments(), undefined);
assert.sameValue(strictAssignArguments(obj), obj);
assert.sameValue(strictAssignArguments(17), 17);

function strictAssignParameterGetElement(a)
{
  "use strict";
  a = 17;
  return arguments[0];
}

assert.sameValue(strictAssignParameterGetElement(42), 42);

function strictAssignArgSub(x, y)
{
  "use strict";
  arguments[0] = 3;
  return arguments[0];
}

assert.sameValue(strictAssignArgSub(1), 3);

function strictAssignArgSubParamUse(x, y)
{
  "use strict";
  arguments[0] = 3;
  assert.sameValue(x, 1);
  return arguments[0];
}

assert.sameValue(strictAssignArgSubParamUse(1), 3);

function strictAssignArgumentsElement(x, y)
{
  "use strict";
  arguments[0] = 3;
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assert.sameValue(strictAssignArgumentsElement(1), 3);

function strictAssignArgumentsElementParamUse(x, y)
{
  "use strict";
  arguments[0] = 3;
  assert.sameValue(x, 1);
  return arguments[Math.random() ? "0" : 0]; // nix arguments[const] optimizations
}

assert.sameValue(strictAssignArgumentsElementParamUse(1), 3);

function strictNestedAssignShadowVar(p)
{
  "use strict";
  function inner()
  {
    var p = 12;
    function innermost() { p = 1776; return 12; }
    return innermost();
  }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowVar(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowVar(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowVar(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowVar(obj), [obj]), true);

function strictNestedAssignShadowCatch(p)
{
  "use strict";
  function inner()
  {
    try
    {
    }
    catch (p)
    {
      var f = function innermost() { p = 1776; return 12; };
      f();
    }
  }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowCatch(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatch(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatch(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatch(obj), [obj]), true);

function strictNestedAssignShadowCatchCall(p)
{
  "use strict";
  function inner()
  {
    try
    {
    }
    catch (p)
    {
      var f = function innermost() { p = 1776; return 12; };
      f();
    }
  }
  inner();
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowCatchCall(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatchCall(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatchCall(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowCatchCall(obj), [obj]), true);

function strictNestedAssignShadowFunction(p)
{
  "use strict";
  function inner()
  {
    function p() { }
    p = 1776;
  }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowFunction(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunction(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunction(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunction(obj), [obj]), true);

function strictNestedAssignShadowFunctionCall(p)
{
  "use strict";
  function inner()
  {
    function p() { }
    p = 1776;
  }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionCall(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionCall(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionCall(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionCall(obj), [obj]), true);

function strictNestedShadowAndMaybeEval(code, p)
{
  "use strict";
  function inner(p) { eval(code); }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("1", 2), ["1", 2]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("arguments"), ["arguments"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("p = 2"), ["p = 2"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("p = 2", 17), ["p = 2", 17]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndMaybeEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictNestedShadowAndEval(code, p)
{
  "use strict";
  function inner(p) { eval(code); }
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedShadowAndEval("1", 2), ["1", 2]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndEval("arguments"), ["arguments"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndEval("p = 2"), ["p = 2"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndEval("p = 2", 17), ["p = 2", 17]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndEval("arguments[0] = 17"), ["arguments[0] = 17"]), true);
assert.sameValue(arraysEqual(strictNestedShadowAndEval("arguments[0] = 17", 42), ["arguments[0] = 17", 42]), true);

function strictEvalContainsMutation(code)
{
  "use strict";
  return eval(code);
}

assert.sameValue(arraysEqual(strictEvalContainsMutation("code = 17; arguments"), ["code = 17; arguments"]), true);
assert.sameValue(arraysEqual(strictEvalContainsMutation("arguments[0] = 17; arguments"), [17]), true);
assert.sameValue(strictEvalContainsMutation("arguments[0] = 17; code"), "arguments[0] = 17; code");

function strictNestedAssignShadowFunctionName(p)
{
  "use strict";
  function inner()
  {
    function p() { p = 1776; }
    p();
  }
  inner();
  return arguments;
}

assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionName(), []), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionName(99), [99]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionName(""), [""]), true);
assert.sameValue(arraysEqual(strictNestedAssignShadowFunctionName(obj), [obj]), true);


/******************************************************************************/

print("All tests passed!");
