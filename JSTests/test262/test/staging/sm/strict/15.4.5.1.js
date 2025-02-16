/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-strict-shell.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var out = {};

function arr() {
  return Object.defineProperty([1, 2, 3, 4], 2, {configurable: false});
}

function nonStrict1(out)
{
  var a = out.array = arr();
  a.length = 2;
}

function strict1(out)
{
  "use strict";
  var a = out.array = arr();
  a.length = 2;
  return a;
}

out.array = null;
nonStrict1(out);
assert.sameValue(deepEqual(out.array, [1, 2, 3]), true);

out.array = null;
try
{
  strict1(out);
  throw "no error";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true, "expected TypeError, got " + e);
}
assert.sameValue(deepEqual(out.array, [1, 2, 3]), true);

// Internally, SpiderMonkey has two representations for arrays:
// fast-but-inflexible, and slow-but-flexible. Adding a non-index property
// to an array turns it into the latter. We should test on both kinds.
function addx(obj) {
  obj.x = 5;
  return obj;
}

function nonStrict2(out)
{
  var a = out.array = addx(arr());
  a.length = 2;
}

function strict2(out)
{
  "use strict";
  var a = out.array = addx(arr());
  a.length = 2;
}

out.array = null;
nonStrict2(out);
assert.sameValue(deepEqual(out.array, addx([1, 2, 3])), true);

out.array = null;
try
{
  strict2(out);
  throw "no error";
}
catch (e)
{
  assert.sameValue(e instanceof TypeError, true, "expected TypeError, got " + e);
}
assert.sameValue(deepEqual(out.array, addx([1, 2, 3])), true);

print("Tests complete");
