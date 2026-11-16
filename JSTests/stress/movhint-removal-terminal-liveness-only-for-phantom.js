function f(a, b, c, s) {
    if (c < 0 || s < 0)
        return -1;
    let x = Math.sqrt(a) * Math.sqrt(b);
    if (c) {
        if (typeof s === "string")
            return x;
        return 1;
    }
    return 2;
}
noInline(f);

function g(a, c, o) {
    let x = a * 3 + 1;
    let y = { x };
    if (c) {
        if (typeof o === "string")
            return y.x + x;
        return o.f;
    }
    return 2;
}
noInline(g);

for (let i = 0; i < testLoopCount; ++i) {
    let r = f(i + 0.5, i + 1.5, i & 1, i);
    if (r !== ((i & 1) ? 1 : 2))
        throw new Error("bad f: " + i + " " + r);
    r = g(i, i & 1, { f: i });
    if (r !== ((i & 1) ? i : 2))
        throw new Error("bad g: " + i + " " + r);
}

let expected = Math.sqrt(4) * Math.sqrt(9);
let r = f(4, 9, 1, "s");
if (r !== expected)
    throw new Error("bad f with string: " + r);
r = g(5, 1, "s");
if (r !== 32)
    throw new Error("bad g with string: " + r);
r = g(5, 1, { get f() { return 7; } });
if (r !== 7)
    throw new Error("bad g with getter: " + r);
