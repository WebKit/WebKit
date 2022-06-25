'use strict';

function _f(a1, a2, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) {
    a *= 1.1;
    b *= 1.2;
    c *= 1.3;
    d *= 1.4;
    e *= 1.5;
    f *= 1.6;
    g *= 1.7;
    h *= 1.8;
    i *= 1.9;
    j *= 2.1;
    k *= 2.2;
    l *= 2.3;
    m *= 2.4;
    n *= 2.5;
    o *= 2.6;

    a1[0] = a;
    a1[1] = b;
    a1[2] = c;
    a1[3] = d;
    a1[4] = e;
    a1[5] = f;
    a1[6] = g;
    a1[7] = h;
    a1[8] = i;
    a1[9] = j;
    a1[10] = k;
    a1[11] = l;
    a1[12] = m;
    a1[13] = n;
    a1[14] = o;

    _g(a1, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);

    a2[0] = a;
    a2[1] = b;
    a2[2] = c;
    a2[3] = d;
    a2[4] = e;
    a2[5] = f;
    a2[6] = g;
    a2[7] = h;
    a2[8] = i;
    a2[9] = j;
    a2[10] = k;
    a2[11] = l;
    a2[12] = m;
    a2[13] = n;
    a2[14] = o;
}
noInline(_f);

function _g(x, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) {
    a *= 1.1;
    b *= 1.2;
    c *= 1.3;
    d *= 1.4;
    e *= 1.5;
    f *= 1.6;
    g *= 1.7;
    h *= 1.8;
    i *= 1.9;
    j *= 2.1;
    k *= 2.2;
    l *= 2.3;
    m *= 2.4;
    n *= 2.5;
    o *= 2.6;

    x[15] = a + b + c + d + e + f + g + h + i + j + k + l + m + n + o;
    _i(x);
    return _h(x, ...[a, b, c, d, e, f, g, h, i, j, k, l, m, n, o]);
}
noInline(_g);

function _h(x, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) {
}
noInline(_h);

function _i() { }
noInline(_i);

function assertEqual(x, y) {
    if (x !== y)
        throw new Error(`assertEqual: fail: ${x} !== ${y}`);
}
noInline(assertEqual);

const count = 15;
let args = [];
for (let i = 1; i <= count; ++i)
    args.push(i);

for (let i = 0; i < 1e5; ++i) {
    let a1 = new Float64Array(count);
    let a2 = new Float64Array(count);
    _f(a1, a2, ...args);
    for (let j = 0; j < count; ++j)
        assertEqual(a1[j], a2[j]);
}
