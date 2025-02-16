/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
info: |
  needs newGlobal()
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 608473;
var summary =
  '|var eval = otherWindow.eval; eval(...)| should behave like indirectly ' +
  'calling that eval from a script in that other window';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var originalEval = eval;
var res;

function f()
{
  return [this, eval("this")];
}

var otherGlobalSameCompartment = createNewGlobal("same-compartment");

eval = otherGlobalSameCompartment.eval;
res = new f();
assert.sameValue(res[0] !== res[1], true);
assert.sameValue(res[0] !== this, true);
assert.sameValue(res[0] instanceof f, true);
assert.sameValue(res[1], otherGlobalSameCompartment);

res = f();
assert.sameValue(res[0] !== res[1], true);
assert.sameValue(res[0], this);
assert.sameValue(res[1], otherGlobalSameCompartment);

var otherGlobalDifferentCompartment = createNewGlobal();

eval = otherGlobalDifferentCompartment.eval;
res = new f();
assert.sameValue(res[0] !== res[1], true);
assert.sameValue(res[0] !== this, true);
assert.sameValue(res[0] instanceof f, true);
assert.sameValue(res[1], otherGlobalDifferentCompartment);

res = f();
assert.sameValue(res[0] !== res[1], true);
assert.sameValue(res[0], this);
assert.sameValue(res[1], otherGlobalDifferentCompartment);

/******************************************************************************/

print("All tests passed!");
