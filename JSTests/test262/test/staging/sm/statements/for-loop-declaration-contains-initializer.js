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
var gTestfile = "for-loop-declaration-contains-computed-name.js";
var BUGNUMBER = 1233767;
var summary =
  "Support initializer defaults in destructuring declarations in for-in/of " +
  "loop heads";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var count;
var expr;

expr = [{ z: 42, 42: "hi" }, { 7: 'fnord' }];
count = 0;
for (var { z: x = 7, [x]: y = 3 } of expr)
{
  if (count === 0) {
    assert.sameValue(x, 42);
    assert.sameValue(y, "hi");
  } else {
    assert.sameValue(x, 7);
    assert.sameValue(y, "fnord");
  }

  count++;
}

count = 0;
for (var { length: x, [x - 1 + count]: y = "psych" } in "foo")
{
  assert.sameValue(x, 1);
  assert.sameValue(y, count === 0 ? "0" : "psych");

  count++;
}

/******************************************************************************/

print("Tests complete");
