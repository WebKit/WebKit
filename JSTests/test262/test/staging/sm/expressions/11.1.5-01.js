/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 520696;
var summary =
  'Implement support for string literals as names for properties defined ' +
  'using ES5 get/set syntax';

print(BUGNUMBER + ": " + summary);


var o;

o = { get "a b c"() { return 17; } };
assert.sameValue("get" in Object.getOwnPropertyDescriptor(o, "a b c"), true);

o = eval('({ get "a b c"() { return 17; } })');
assert.sameValue("get" in Object.getOwnPropertyDescriptor(o, "a b c"), true);

var f = eval("(function literalInside() { return { set 'c d e'(q) { } }; })");
f = function literalInside() { return { set 'c d e'(q) { } }; };

function checkO()
{
  assert.sameValue(3.141592654 in o, true, "fractional-named property isn't in object");
  assert.sameValue(10000 in o, true, "exponential-named property is in object");
  assert.sameValue(0xdeadbeef in o, true, "hex-named property is in object");
  assert.sameValue("Infinity" in o, true, "numeric index stringified correctly");
}

o = eval('({ 3.141592654: "pi", 1e4: 17, 0xdeadbeef: "hex", 1e3000: "Infinity" })');
checkO();
o = { 3.141592654: "pi", 1e4: 17, 0xdeadbeef: "hex", 1e3000: "Infinity" };
checkO();

