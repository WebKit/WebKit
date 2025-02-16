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
var symbols = [
    Symbol(),
    Symbol("0"),
    Symbol.for("snowman"),
    Symbol.iterator
];

var values = [
    undefined, null, 0, 3.14, -0, NaN, "", "alphabet", Symbol("0"),
    {}, []
];

for (var comparator of ["==", "!=", "===", "!=="]) {
    var f = Function("a, b", "return a " + comparator + " b;");
    var expected = (comparator[0] == '!');
    for (var a of symbols) {
        for (var b of values)
            assert.sameValue(f(a, b), expected);
    }
}

for (var comparator of ["<", "<=", ">", ">="]) {
    var f = Function("a, b", "return a " + comparator + " b;");
    for (var a of symbols) {
        for (var b of values)
            assertThrowsInstanceOf(() => f(a, b), TypeError);
    }
}

