// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Section numbers cite ES6 rev 24 (2014 April 27).

var symbols = [
    Symbol(),
    Symbol("one"),
    Symbol.for("two"),
    Symbol.iterator
];

function testSymbolConversions(sym) {
    // 7.1.2 ToBoolean
    assert.sameValue(Boolean(sym), true);
    assert.sameValue(!sym, false);
    assert.sameValue(sym || 13, sym);
    assert.sameValue(sym && 13, 13);

    // 7.1.3 ToNumber
    assertThrowsInstanceOf(() => +sym, TypeError);
    assertThrowsInstanceOf(() => sym | 0, TypeError);

    // 7.1.12 ToString
    assertThrowsInstanceOf(() => "" + sym, TypeError);
    assertThrowsInstanceOf(() => sym + "", TypeError);
    assertThrowsInstanceOf(() => "" + [1, 2, sym], TypeError);
    assertThrowsInstanceOf(() => ["simple", "thimble", sym].join(), TypeError);

    // 21.1.1.1 String()
    assert.sameValue(String(sym), sym.toString());

    // 21.1.1.2 new String()
    assertThrowsInstanceOf(() => new String(sym), TypeError);

    // 7.1.13 ToObject
    var obj = Object(sym);
    assert.sameValue(typeof obj, "object");
    assert.sameValue(Object.prototype.toString.call(obj), "[object Symbol]");
    assert.sameValue(Object.getPrototypeOf(obj), Symbol.prototype);
    assert.sameValue(Object.getOwnPropertyNames(obj).length, 0);
    assert.sameValue(Object(sym) === Object(sym), false);  // new object each time
    var f = function () { return this; };
    assert.sameValue(f.call(sym) === f.call(sym), false);  // new object each time
}


for (var sym of symbols) {
    testSymbolConversions(sym);

    // 7.1.1 ToPrimitive
    var symobj = Object(sym);
    assertThrowsInstanceOf(() => Number(symobj), TypeError);
    assertThrowsInstanceOf(() => String(symobj), TypeError);
    assertThrowsInstanceOf(() => symobj < 0, TypeError);
    assertThrowsInstanceOf(() => 0 < symobj, TypeError);
    assertThrowsInstanceOf(() => symobj + 1, TypeError);
    assertThrowsInstanceOf(() => "" + symobj, TypeError);
    assert.sameValue(sym == symobj, true);
    assert.sameValue(sym === symobj, false);
    assert.sameValue(symobj == 0, false);
    assert.sameValue(0 != symobj, true);

    // 7.1.12 ToString
    assertThrowsInstanceOf(() => String(Object(sym)), TypeError);
}

// Deleting Symbol.prototype[@@toPrimitive] does not change the behavior of
// conversions from a symbol to other types.
delete Symbol.prototype[Symbol.toPrimitive];
assert.sameValue(Symbol.toPrimitive in Symbol.prototype, false);
testSymbolConversions(symbols[0]);

// It does change the behavior of ToPrimitive on Symbol objects, though.
// It causes the default algorithm (OrdinaryToPrimitive) to be used.
var VALUEOF_CALLED = 117.25;
Symbol.prototype.valueOf =  function () { return VALUEOF_CALLED; };
Symbol.prototype.toString = function () { return "toString called"; };
for (var sym of symbols) {
    var symobj = Object(sym);
    assert.sameValue(Number(symobj), VALUEOF_CALLED);
    assert.sameValue(String(symobj), "toString called");
    assert.sameValue(symobj < 0, VALUEOF_CALLED < 0);
    assert.sameValue(0 < symobj, 0 < VALUEOF_CALLED);
    assert.sameValue(symobj + 1, VALUEOF_CALLED + 1);
    assert.sameValue("" + symobj, "" + VALUEOF_CALLED);
    assert.sameValue(symobj == 0, false);
    assert.sameValue(0 != symobj, true);
}

