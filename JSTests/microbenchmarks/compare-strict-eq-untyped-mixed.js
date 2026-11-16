function eq(a, b) {
    return a === b;
}
noInline(eq);

var obj1 = {};
var obj2 = {};
var str1 = "hello" + Math.random().toString().slice(2);
var str2 = "hello" + Math.random().toString().slice(2);

var pairs = [
    [null, null, true],
    [undefined, undefined, true],
    [null, undefined, false],
    [undefined, null, false],
    [true, true, true],
    [false, false, true],
    [true, false, false],
    [null, 0, false],
    [null, false, false],
    [undefined, 0, false],
    [5, 5, true],
    [5, 7, false],
    [0, 0, true],
    [-0, 0, true],
    [obj1, obj1, true],
    [obj1, obj2, false],
    [obj1, null, false],
    [obj1, 42, false],
    [1.5, 1.5, true],
    [1.5, 2.5, false],
    [NaN, NaN, false],
    [0.1 + 0.2, 0.3, false],
    [str1, str1, true],
    [str1, str2, false],
    ["ab" + "c", "abc", true],
    [1n, 1n, true],
    [1n, 2n, false],
    [Symbol.iterator, Symbol.iterator, true],
];

for (var iter = 0; iter < 1e6; ++iter) {
    for (var i = 0; i < pairs.length; ++i) {
        var p = pairs[i];
        var got = eq(p[0], p[1]);
        if (got !== p[2])
            throw new Error("iter " + iter + " pair " + i + ": eq(" + String(p[0]) + ", " + String(p[1]) + ") = " + got + ", expected " + p[2]);
    }
}

var fastPairs = [
    [null, null, true],
    [undefined, undefined, true],
    [null, undefined, false],
    [true, false, false],
    [obj1, obj1, true],
    [obj1, obj2, false],
    [5, 5, true],
    [5, 7, false],
];
var acc = 0;
for (var iter = 0; iter < 1e6; ++iter) {
    for (var i = 0; i < fastPairs.length; ++i) {
        var p = fastPairs[i];
        if (eq(p[0], p[1]))
            acc++;
    }
}
if (acc !== 1e6 * 4)
    throw new Error("fastPairs acc mismatch: " + acc);
