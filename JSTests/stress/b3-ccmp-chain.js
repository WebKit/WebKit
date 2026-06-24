// Regression tests for B3 ccmp-chain lowering of nested boolean expressions on ARM64.

let arr = [0, 1, 2, 3, 4, 5, 6, 7];

function f(arr, i, a, b, c, d) {
    if (i < 0)
        return -2;
    if ((i < arr.length) & (((((a == b) & (c == d)) | (a ^ c)) == 0))) {
        return arr[i];
    }
    return -1;
}
noInline(f);
for (let j = 0; j < testLoopCount; j++) {
    let r1 = f(arr, j & 7, 5, 6, 5, 5);
    if (r1 !== arr[j & 7])
        throw new Error("T1 mismatch: f(arr, " + (j & 7) + ", 5, 6, 5, 5) got " + r1);
    let r2 = f(arr, j & 7, 5, 5, 5, 5);
    if (r2 !== -1)
        throw new Error("T1 mismatch: f(arr, " + (j & 7) + ", 5, 5, 5, 5) got " + r2);
}
// T1
if (f(arr, 0x7ffffff0, 5, 5, 5, -10) !== -1) throw new Error("T1 mismatch case 1");
if (f(arr, 0x7ffffff0, 5, 5, 5, 10) !== -1) throw new Error("T1 mismatch case 2");
if (f(arr, 5, 5, 6, 5, 5) !== arr[5]) throw new Error("T1 mismatch case 3");
if (f(arr, 5, 5, 5, 5, 5) !== -1) throw new Error("T1 mismatch case 4");

function f_legit_chain(a, b, c, d, e, g) {
    if ((a == b) & (c < d) & (e != g))
        return 1;
    return 0;
}
noInline(f_legit_chain);
let cases = [
    [5, 5, 1, 2, 3, 4, 1],
    [5, 5, 2, 1, 3, 4, 0],
    [5, 6, 1, 2, 3, 4, 0],
    [5, 5, 1, 2, 3, 3, 0],
    [0, 0, 0, 1, 0, 0, 0],
    [-1, -1, -3, -2, 1, 0, 1],
];
for (let j = 0; j < testLoopCount; j++) {
    for (let [a, b, c, d, e, g] of cases)
        f_legit_chain(a, b, c, d, e, g);
}
// T2
for (let [a, b, c, d, e, g, exp] of cases) {
    let got = f_legit_chain(a, b, c, d, e, g);
    if (got !== exp)
        throw new Error("T2 regression: f_legit_chain(" + [a, b, c, d, e, g] + ") got " + got + " expected " + exp);
}
