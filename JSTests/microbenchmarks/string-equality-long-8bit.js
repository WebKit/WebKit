function makeString(len, ch) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += ch;
    return s;
}

// Two distinct JSString cells with identical 8-bit content (avoid pointer-equal fast path).
let a = makeString(64, "a") + "x";
let b = makeString(64, "a") + "x";
let c = makeString(64, "a") + "y";

function eq(x, y) {
    return x === y;
}
noInline(eq);

let n = 0;
for (let i = 0; i < 1e6; ++i) {
    if (eq(a, b))
        n++;
    if (eq(a, c))
        n++;
}
if (n !== 1e6)
    throw "Bad result: " + n;
