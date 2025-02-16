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
//-----------------------------------------------------------------------------
var BUGNUMBER = 537863;
var summary =
  'undefined, Infinity, and NaN global properties should not be writable';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var desc, old, error;
var global = this;

var names = ["NaN", "Infinity", "undefined"];

for (var i = 0; i < names.length; i++)
{
  var name = names[i];
  desc = Object.getOwnPropertyDescriptor(global, name);
  assert.sameValue(desc !== undefined, true, name + " should be present");
  assert.sameValue(desc.enumerable, false, name + " should not be enumerable");
  assert.sameValue(desc.configurable, false, name + " should not be configurable");
  assert.sameValue(desc.writable, false, name + " should not be writable");

  old = global[name];
  global[name] = 17;
  assert.sameValue(global[name], old, name + " changed on setting?");

  error = "before";
  try
  {
    throw new TypeError("SpiderMonkey doesn't currently implement " +
                        "strict-mode throwing when setting a readonly " +
                        "property, not running this bit of test for now; " +
                        "see bug 537873");

    (function() { "use strict"; global[name] = 42; error = "didn't throw"; })();
  }
  catch (e)
  {
    if (e instanceof TypeError)
      error = "typeerror";
    else
      error = "bad exception: " + e;
  }
  assert.sameValue(error, "typeerror", "wrong strict mode error setting " + name);
}

/******************************************************************************/

print("All tests passed!");
