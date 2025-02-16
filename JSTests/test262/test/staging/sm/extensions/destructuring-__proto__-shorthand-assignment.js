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
var gTestfile = 'destructuring-__proto__-shorthand-assignment.js';
var BUGNUMBER = 963641;
var summary = "{ __proto__ } should work as a destructuring assignment pattern";

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

var { __proto__ } = objectWithProtoProperty(42);
assert.sameValue(__proto__, 42);

({ __proto__ } = objectWithProtoProperty(17));
assert.sameValue(__proto__, 17);

function nested()
{
  var { __proto__ } = objectWithProtoProperty("fnord");
  assert.sameValue(__proto__, "fnord");

  ({ __proto__ } = objectWithProtoProperty(undefined));
  assert.sameValue(__proto__, undefined);
}
nested();

/******************************************************************************/

print("Tests complete");
