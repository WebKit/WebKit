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
function foo()
{
  assert.sameValue(foo.arguments.length, 0);
  assert.sameValue(foo.caller, null);
}

assert.sameValue(foo.arguments, null);
assert.sameValue(foo.caller, null);
foo();
assert.sameValue(foo.arguments, null);
assert.sameValue(foo.caller, null);

/******************************************************************************/

print("Tests complete");
