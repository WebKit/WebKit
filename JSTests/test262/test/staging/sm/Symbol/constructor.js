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
// Symbol(symbol) throws a TypeError.
var sym = Symbol();
assertThrowsInstanceOf(() => Symbol(sym), TypeError);

// Symbol(undefined) is equivalent to Symbol().
assert.sameValue(Symbol(undefined).toString(), "Symbol()");

// Otherwise, Symbol(v) means Symbol(ToString(v)).
assert.sameValue(Symbol(7).toString(), "Symbol(7)");
assert.sameValue(Symbol(true).toString(), "Symbol(true)");
assert.sameValue(Symbol(null).toString(), "Symbol(null)");
assert.sameValue(Symbol([1, 2]).toString(), "Symbol(1,2)");
var symobj = Object(sym);
assertThrowsInstanceOf(() => Symbol(symobj), TypeError);

var hits = 0;
var obj = {
    toString: function () {
        hits++;
        return "ponies";
    }
};
assert.sameValue(Symbol(obj).toString(), "Symbol(ponies)");
assert.sameValue(hits, 1);

assert.sameValue(Object.getPrototypeOf(Symbol.prototype), Object.prototype);

// Symbol.prototype is not itself a Symbol object.
assertThrowsInstanceOf(() => Symbol.prototype.valueOf(), TypeError);

