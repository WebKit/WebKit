// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-Date-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// ES6 20.3.4.45 Date.prototype[@@toPrimitive](hint)

// The toPrimitive method throws if the this value isn't an object.
var toPrimitive = Date.prototype[Symbol.toPrimitive];
assertThrowsInstanceOf(() => toPrimitive.call(undefined, "default"), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(3, "default"), TypeError);

// It doesn't have to be a Date object, though.
var obj = {
    toString() { return "str"; },
    valueOf() { return "val"; }
};
assert.sameValue(toPrimitive.call(obj, "number"), "val");
assert.sameValue(toPrimitive.call(obj, "string"), "str");
assert.sameValue(toPrimitive.call(obj, "default"), "str");

// It throws if the hint argument is missing or not one of the three allowed values.
assertThrowsInstanceOf(() => toPrimitive.call(obj), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, undefined), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, "boolean"), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, ["number"]), TypeError);
assertThrowsInstanceOf(() => toPrimitive.call(obj, {toString() { throw "FAIL"; }}), TypeError);

// The next few tests cover the OrdinaryToPrimitive algorithm, specified in
// ES6 7.1.1 ToPrimitive(input [, PreferredType]).

// Date.prototype.toString or .valueOf can be overridden.
var dateobj = new Date();
Date.prototype.toString = function () {
    assert.sameValue(this, dateobj);
    return 14;
};
Date.prototype.valueOf = function () {
    return "92";
};
assert.sameValue(dateobj[Symbol.toPrimitive]("number"), "92");
assert.sameValue(dateobj[Symbol.toPrimitive]("string"), 14);
assert.sameValue(dateobj[Symbol.toPrimitive]("default"), 14);
assert.sameValue(dateobj == 14, true);  // equality comparison: passes "default"

// If this.toString is a non-callable value, this.valueOf is called instead.
Date.prototype.toString = {};
assert.sameValue(dateobj[Symbol.toPrimitive]("string"), "92");
assert.sameValue(dateobj[Symbol.toPrimitive]("default"), "92");

// And vice versa.
Date.prototype.toString = function () { return 15; };
Date.prototype.valueOf = "ponies";
assert.sameValue(dateobj[Symbol.toPrimitive]("number"), 15);

// If neither is callable, it throws a TypeError.
Date.prototype.toString = "ponies";
assertThrowsInstanceOf(() => dateobj[Symbol.toPrimitive]("default"), TypeError);

// Surface features.
assert.sameValue(toPrimitive.name, "[Symbol.toPrimitive]");
var desc = Object.getOwnPropertyDescriptor(Date.prototype, Symbol.toPrimitive);
assert.sameValue(desc.configurable, true);
assert.sameValue(desc.enumerable, false);
assert.sameValue(desc.writable, false);

