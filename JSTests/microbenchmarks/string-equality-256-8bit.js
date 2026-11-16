function makeString(len, ch) {
    let s = "";
    for (let i = 0; i < len; ++i)
        s += ch;
    return s;
}

let a = makeString(255, "a") + "x";
let b = makeString(255, "a") + "x";
let c = makeString(255, "a") + "y";

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
