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
var f;
try
{
  f = eval("(function literalInside() { return { set 'c d e'(v) { } }; })");
}
catch (e)
{
  assert.sameValue(true, false,
           "string-literal property name in setter in object literal in " +
           "function statement failed to parse: " + e);
}

var fstr = "" + f;
assert.sameValue(fstr.indexOf("set") < fstr.indexOf("c d e"), true,
         "should be using new-style syntax with string literal in place of " +
         "property identifier");
assert.sameValue(fstr.indexOf("setter") < 0, true, "using old-style syntax?");

var o = f();
var ostr = "" + o;
assert.sameValue("c d e" in o, true, "missing the property?");
assert.sameValue("set" in Object.getOwnPropertyDescriptor(o, "c d e"), true,
         "'c d e' property not a setter?");
// disabled because we still generate old-style syntax here (toSource
// decompilation is as yet unfixed)
// assert.sameValue(ostr.indexOf("set") < ostr.indexOf("c d e"), true,
//         "should be using new-style syntax when getting the source of a " +
//         "getter/setter while decompiling an object");
// assert.sameValue(ostr.indexOf("setter") < 0, true, "using old-style syntax?");

