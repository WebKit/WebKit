function makeString(len, ch) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += ch;
    return s;
}

// Embedding a non-Latin1 character forces 16-bit storage.
let a = makeString(2, "α") + "β";
let b = makeString(2, "α") + "β";
let c = makeString(2, "α") + "γ";

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
