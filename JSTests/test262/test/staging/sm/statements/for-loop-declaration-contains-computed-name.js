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
  "Support computed property names in destructuring declarations in " +
  "for-in/of loop heads";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var count;

count = 0;
for (var { [5]: x, [x]: y } of [{ 5: 42, 42: "hi" }, { 5: 17, 17: 'fnord' }])
{
  if (count === 0) {
    assert.sameValue(x, 42);
    assert.sameValue(y, "hi");
  } else {
    assert.sameValue(x, 17);
    assert.sameValue(y, "fnord");
  }

  count++;
}

count = 0;
for (var { length: x, [x - 1]: y } in "foo")
{
  assert.sameValue(x, 1);
  assert.sameValue("" + count, y);

  count++;
}

/******************************************************************************/

print("Tests complete");
