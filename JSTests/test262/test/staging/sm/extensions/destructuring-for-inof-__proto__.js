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
var gTestfile = 'destructuring-for-inof-__proto__.js';
var BUGNUMBER = 963641;
var summary =
  "__proto__ should work in destructuring patterns as the targets of " +
  "for-in/for-of loops";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function objectWithProtoProperty(v)
{
  var obj = {};
  return Object.defineProperty(obj, "__proto__",
                               {
                                 enumerable: true,
                                 configurable: true,
                                 writable: true,
                                 value: v
                               });
}

function* objectWithProtoGenerator(v)
{
  yield objectWithProtoProperty(v);
}

function* identityGenerator(v)
{
  yield v;
}

for (var { __proto__: target } of objectWithProtoGenerator(null))
  assert.sameValue(target, null);

for ({ __proto__: target } of objectWithProtoGenerator("aacchhorrt"))
  assert.sameValue(target, "aacchhorrt");

for ({ __proto__: target } of identityGenerator(42))
  assert.sameValue(target, Number.prototype);

for (var { __proto__: target } in { prop: "kneedle" })
  assert.sameValue(target, String.prototype);

for ({ __proto__: target } in { prop: "snork" })
  assert.sameValue(target, String.prototype);

for ({ __proto__: target } in { prop: "ohia" })
  assert.sameValue(target, String.prototype);

function nested()
{
  for (var { __proto__: target } of objectWithProtoGenerator(null))
    assert.sameValue(target, null);

  for ({ __proto__: target } of objectWithProtoGenerator("aacchhorrt"))
    assert.sameValue(target, "aacchhorrt");

  for ({ __proto__: target } of identityGenerator(42))
    assert.sameValue(target, Number.prototype);

  for (var { __proto__: target } in { prop: "kneedle" })
    assert.sameValue(target, String.prototype);

  for ({ __proto__: target } in { prop: "snork" })
    assert.sameValue(target, String.prototype);

  for ({ __proto__: target } in { prop: "ohia" })
    assert.sameValue(target, String.prototype);
}
nested();

/******************************************************************************/

print("Tests complete");
