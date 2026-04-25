function eq(a, b) {
    return a === b;
}
noInline(eq);

var o1 = {};
var o2 = {};
for (var i = 0; i < 1e6; ++i) {
    eq(1.5, 2.5);
    eq(null, null);
    eq(undefined, undefined);
    eq(true, false);
    eq(5, 7);
    eq(o1, o2);
}

var N = 1e6;
var acc = 0;
var a = null;
var b = undefined;
var c = true;
var d = false;
for (var i = 0; i < N; ++i) {
    if (eq(a, a)) acc++;
    if (eq(b, b)) acc++;
    if (eq(a, b)) acc++;
    if (eq(c, c)) acc++;
    if (eq(c, d)) acc++;
}
if (acc !== N * 3)
    throw new Error("bad acc: " + acc);
