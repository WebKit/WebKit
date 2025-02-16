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
var BUGNUMBER = 732669;
var summary = "Primitive values don't box correctly";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var t;
function returnThis() { return this; }

// Boolean

Boolean.prototype.method = returnThis;
t = true.method();
assert.sameValue(t !== Boolean.prototype, true);
assert.sameValue(t.toString(), "true");

Object.defineProperty(Boolean.prototype, "property", { get: returnThis, configurable: true });
t = false.property;
assert.sameValue(t !== Boolean.prototype, true);
assert.sameValue(t.toString(), "false");

delete Boolean.prototype.method;
delete Boolean.prototype.property;


// Number

Number.prototype.method = returnThis;
t = 5..method();
assert.sameValue(t !== Number.prototype, true);
assert.sameValue(t.toString(), "5");

Object.defineProperty(Number.prototype, "property", { get: returnThis, configurable: true });
t = 17..property;
assert.sameValue(t !== Number.prototype, true);
assert.sameValue(t.toString(), "17");

delete Number.prototype.method;
delete Number.prototype.property;


// String

String.prototype.method = returnThis;
t = "foo".method();
assert.sameValue(t !== String.prototype, true);
assert.sameValue(t.toString(), "foo");

Object.defineProperty(String.prototype, "property", { get: returnThis, configurable: true });
t = "bar".property;
assert.sameValue(t !== String.prototype, true);
assert.sameValue(t.toString(), "bar");

delete String.prototype.method;
delete String.prototype.property;


// Object

Object.prototype.method = returnThis;

t = true.method();
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== Boolean.prototype, true);
assert.sameValue(t.toString(), "true");

t = 42..method();
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== Number.prototype, true);
assert.sameValue(t.toString(), "42");

t = "foo".method();
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== String.prototype, true);
assert.sameValue(t.toString(), "foo");

Object.defineProperty(Object.prototype, "property", { get: returnThis, configurable: true });

t = false.property;
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== Boolean.prototype, true);
assert.sameValue(t.toString(), "false");

t = 8675309..property;
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== Number.prototype, true);
assert.sameValue(t.toString(), "8675309");

t = "bar".property;
assert.sameValue(t !== Object.prototype, true);
assert.sameValue(t !== String.prototype, true);
assert.sameValue(t.toString(), "bar");

/******************************************************************************/

print("Tests complete");
