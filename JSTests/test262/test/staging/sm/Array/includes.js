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
var BUGNUMBER = 1069063;
var summary = "Implement Array.prototype.includes";

print(BUGNUMBER + ": " + summary);

assert.sameValue(typeof [].includes, "function");
assert.sameValue([].includes.length, 1);

assertTrue([1, 2, 3].includes(2));
assertTrue([1,,2].includes(2));
assertTrue([1, 2, 3].includes(2, 1));
assertTrue([1, 2, 3].includes(2, -2));
assertTrue([1, 2, 3].includes(2, -100));
assertTrue([Object, Function, Array].includes(Function));
assertTrue([-0].includes(0));
assertTrue([NaN].includes(NaN));
assertTrue([,].includes());
assertTrue(staticIncludes("123", "2"));
assertTrue(staticIncludes({length: 3, 1: 2}, 2));
assertTrue(staticIncludes({length: 3, 1: 2, get 3(){throw ""}}, 2));
assertTrue(staticIncludes({length: 3, get 1() {return 2}}, 2));
assertTrue(staticIncludes({__proto__: {1: 2}, length: 3}, 2));
assertTrue(staticIncludes(new Proxy([1], {get(){return 2}}), 2));

assertFalse([1, 2, 3].includes("2"));
assertFalse([1, 2, 3].includes(2, 2));
assertFalse([1, 2, 3].includes(2, -1));
assertFalse([undefined].includes(NaN));
assertFalse([{}].includes({}));
assertFalse(staticIncludes({length: 3, 1: 2}, 2, 2));
assertFalse(staticIncludes({length: 3, get 0(){delete this[1]}, 1: 2}, 2));
assertFalse(staticIncludes({length: -100, 0: 1}, 1));

assertThrowsInstanceOf(() => staticIncludes(), TypeError);
assertThrowsInstanceOf(() => staticIncludes(null), TypeError);
assertThrowsInstanceOf(() => staticIncludes({get length(){throw TypeError()}}), TypeError);
assertThrowsInstanceOf(() => staticIncludes({length: 3, get 1() {throw TypeError()}}, 2), TypeError);
assertThrowsInstanceOf(() => staticIncludes({__proto__: {get 1() {throw TypeError()}}, length: 3}, 2), TypeError);
assertThrowsInstanceOf(() => staticIncludes(new Proxy([1], {get(){throw TypeError()}})), TypeError);

function assertTrue(v) {
    assert.sameValue(v, true);
}

function assertFalse(v) {
    assert.sameValue(v, false);
}

function staticIncludes(o, v, f) {
    return [].includes.call(o, v, f);
}

