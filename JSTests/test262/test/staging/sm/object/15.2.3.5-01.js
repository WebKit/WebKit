/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 492840;
var summary = 'ES5 Object.create(O [, Properties])';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assert.sameValue("create" in Object, true);
assert.sameValue(Object.create.length, 2);

var o, desc, props, proto;

o = Object.create(null);
assert.sameValue(Object.getPrototypeOf(o), null, "bad null-proto");

o = Object.create(null, { a: { value: 17, enumerable: false } });
assert.sameValue(Object.getPrototypeOf(o), null, "bad null-proto");
assert.sameValue("a" in o, true);
desc = Object.getOwnPropertyDescriptor(o, "a");
assert.sameValue(desc !== undefined, true, "no descriptor?");
assert.sameValue(desc.value, 17);
assert.sameValue(desc.enumerable, false);
assert.sameValue(desc.configurable, false);
assert.sameValue(desc.writable, false);

props = Object.create({ bar: 15 });
Object.defineProperty(props, "foo", { enumerable: false, value: 42 });
proto = { baz: 12 };
o = Object.create(proto, props);
assert.sameValue(Object.getPrototypeOf(o), proto);
assert.sameValue(Object.getOwnPropertyDescriptor(o, "foo"), undefined);
assert.sameValue("foo" in o, false);
assert.sameValue(Object.getOwnPropertyDescriptor(o, "bar"), undefined);
assert.sameValue("bar" in o, false);
assert.sameValue(Object.getOwnPropertyDescriptor(o, "baz"), undefined);
assert.sameValue(o.baz, 12);
assert.sameValue(o.hasOwnProperty("baz"), false);

try {
  var actual =
    Object.create(Object.create({},
                                { boom: { get: function() { return "base"; }}}),
                  { boom: { get: function() { return "overridden"; }}}).boom
} catch (e) {
}
assert.sameValue(actual, "overridden");

/******************************************************************************/

print("All tests passed!");
