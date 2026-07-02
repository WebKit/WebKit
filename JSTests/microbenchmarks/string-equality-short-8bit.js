function makeString(len, ch) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += ch;
    return s;
}

// length 7: worst case for the byte-at-a-time loop (just below the word threshold).
let a = makeString(6, "a") + "x";
let b = makeString(6, "a") + "x";
let c = makeString(6, "a") + "y";

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
