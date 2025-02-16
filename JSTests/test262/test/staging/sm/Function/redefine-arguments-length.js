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
var gTestfile = 'redefine-arguments-length.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 539766;
var summary =
  "Object.defineProperty sets arguments.length without setting the " +
  "length-overridden bit";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function test_JSOP_ARGCNT()
{
  var length = "length";
  Object.defineProperty(arguments, length, { value: 17 });
  assert.sameValue(arguments.length, 17);
  assert.sameValue(arguments[length], 17);
}
test_JSOP_ARGCNT();

function test_js_fun_apply()
{
  var length = "length";
  Object.defineProperty(arguments, length, { value: 17 });

  function fun()
  {
    assert.sameValue(arguments.length, 17);
    assert.sameValue(arguments[length], 17);
    assert.sameValue(arguments[0], "foo");
    for (var i = 1; i < 17; i++)
      assert.sameValue(arguments[i], undefined);
  }
  fun.apply(null, arguments);
}
test_js_fun_apply("foo");

function test_array_toString_sub_1()
{
  Object.defineProperty(arguments, "length", { value: 1 });
  arguments.join = [].join;
  assert.sameValue([].toString.call(arguments), "1");
}
test_array_toString_sub_1(1, 2);

function test_array_toString_sub_2()
{
  Object.defineProperty(arguments, "length", { value: 1 });
  assert.sameValue([].toLocaleString.call(arguments), "1");
}
test_array_toString_sub_2(1, 2);


/******************************************************************************/

print("All tests passed!");
