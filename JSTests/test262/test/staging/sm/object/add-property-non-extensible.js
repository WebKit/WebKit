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
var gTestfile = 'add-property-non-extensible.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 602144;
var summary =
  'Properly method-compile attempted addition of properties to ' +
  'non-extensible objects';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// No property

var o1 = {};
for (var i = 0; i < 5; i++)
  o1.a = 3;
assert.sameValue(o1.a, 3);

var o2 = Object.preventExtensions({});
for (var i = 0; i < 5; i++)
  o2.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o2, "a"), undefined);

var o3 = Object.seal({});
for (var i = 0; i < 5; i++)
  o3.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o3, "a"), undefined);

var o4 = Object.freeze({});
for (var i = 0; i < 5; i++)
  o4.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o4, "a"), undefined);


// Has property

var o5 = { a: 2 };
for (var i = 0; i < 5; i++)
  o5.a = 3;
assert.sameValue(o5.a, 3);

var o6 = Object.preventExtensions({ a: 2 });
for (var i = 0; i < 5; i++)
  o6.a = 3;
assert.sameValue(o6.a, 3);

var o7 = Object.seal({ a: 2 });
for (var i = 0; i < 5; i++)
  o7.a = 3;
assert.sameValue(o7.a, 3);

var o8 = Object.freeze({ a: 2 });
for (var i = 0; i < 5; i++)
  o8.a = 3;
assert.sameValue(o8.a, 2);


// Extensible, hit up the prototype chain

var o9 = Object.create({ a: 2 });
for (var i = 0; i < 5; i++)
  o9.a = 3;
assert.sameValue(o9.a, 3);

var o10 = Object.create(Object.preventExtensions({ a: 2 }));
for (var i = 0; i < 5; i++)
  o10.a = 3;
assert.sameValue(o10.a, 3);

var o11 = Object.create(Object.seal({ a: 2 }));
for (var i = 0; i < 5; i++)
  o11.a = 3;
assert.sameValue(o11.a, 3);

var o12 = Object.create(Object.freeze({ a: 2 }));
for (var i = 0; i < 5; i++)
  o12.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o12, "a"), undefined);


// Not extensible, hit up the prototype chain

var o13 = Object.preventExtensions(Object.create({ a: 2 }));
for (var i = 0; i < 5; i++)
  o13.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o13, "a"), undefined);

var o14 =
  Object.preventExtensions(Object.create(Object.preventExtensions({ a: 2 })));
for (var i = 0; i < 5; i++)
  o14.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o14, "a"), undefined);

var o15 = Object.preventExtensions(Object.create(Object.seal({ a: 2 })));
for (var i = 0; i < 5; i++)
  o15.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o15, "a"), undefined);

var o16 = Object.preventExtensions(Object.create(Object.freeze({ a: 2 })));
for (var i = 0; i < 5; i++)
  o16.a = 3;
assert.sameValue(Object.getOwnPropertyDescriptor(o16, "a"), undefined);


/******************************************************************************/

print("All tests passed!");
