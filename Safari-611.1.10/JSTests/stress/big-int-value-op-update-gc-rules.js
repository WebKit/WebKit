function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

function doesGCAdd(a) {
    let o = {};
    let c = a + 1n;
    o.b = c;

    return o;
}
noInline(doesGCAdd);

for (var i = 0; i < 10000; i++) {
    let o = doesGCAdd(3n);
    assert(o.b, 4n);
}

function doesGCSub(a) {
    let o = {};
    let c = a - 1n;
    o.b = c;

    return o;
}
noInline(doesGCSub);

for (var i = 0; i < 10000; i++) {
    let o = doesGCSub(3n);
    assert(o.b, 2n);
}

function doesGCDiv(a) {
    let o = {};
    let c = a / 2n;
    o.b = c;

    return o;
}
noInline(doesGCDiv);

for (var i = 0; i < 10000; i++) {
    let o = doesGCDiv(4n);
    assert(o.b, 2n);
}

function doesGCMul(a) {
    let o = {};
    let c = a * 2n;
    o.b = c;

    return o;
}
noInline(doesGCMul);

for (var i = 0; i < 10000; i++) {
    let o = doesGCMul(4n);
    assert(o.b, 8n);
}

function doesGCBitAnd(a) {
    let o = {};
    let c = a & 0b11n;
    o.b = c;

    return o;
}
noInline(doesGCBitAnd);

for (var i = 0; i < 10000; i++) {
    let o = doesGCBitAnd(0b1010n);
    assert(o.b, 0b10n);
}

function doesGCBitOr(a) {
    let o = {};
    let c = a | 0b11n;
    o.b = c;

    return o;
}
noInline(doesGCBitOr);

for (var i = 0; i < 10000; i++) {
    let o = doesGCBitOr(0b10n);
    assert(o.b, 0b11n);
}

function doesGCBitXor(a) {
    let o = {};
    let c = a ^ 0b11n;
    o.b = c;

    return o;
}
noInline(doesGCBitXor);

for (var i = 0; i < 10000; i++) {
    let o = doesGCBitXor(0b10n);
    assert(o.b, 0b1n);
}

