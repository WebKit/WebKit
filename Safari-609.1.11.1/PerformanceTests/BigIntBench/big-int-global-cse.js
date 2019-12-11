function assert(a, e) {
    if (a !== e)
        throw new Error("Expected " + e + " but got: " + a);
}

function bigIntAdd(a, b) {
    let c = a + b;
    if (b) {
        assert(c, a + b);
    }
    return a + b + c;
}
noInline(bigIntAdd);

for (let i = 0; i < 100000; i++) {
    assert(bigIntAdd(3n, 5n), 16n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntAdd(0xffffffffffffffffffn, 0xaaffffffffffffffffffn), 1624494070107157953511420n);
}

function bigIntMul(a, b) {
    let c = a * b;
    if (b) {
        assert(c, a * b);
    }
    return a * b + c;
}
noInline(bigIntMul);

for (let i = 0; i < 100000; i++) {
    assert(bigIntMul(3n, 5n), 30n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntMul(0xffffffffffffffffffn, 0xaaffffffffffffffffffn), 7626854857897473114403591155175632477091790850n);
}

function bigIntDiv(a, b) {
    let c = a / b;
    if (b) {
        assert(c, a / b);
    }
    return a / b + c;
}
noInline(bigIntDiv);

for (let i = 0; i < 100000; i++) {
    assert(bigIntDiv(15n, 5n), 6n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntDiv(0xaaffffffffffffffffffn, 0xffffffffffffffffffn), 342n);
}

function bigIntSub(a, b) {
    let c = a - b;
    if (b) {
        assert(c, a - b);
    }
    return a - b + c;
}
noInline(bigIntSub);

for (let i = 0; i < 100000; i++) {
    assert(bigIntSub(15n, 5n), 20n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntSub(0xaaffffffffffffffffffn, 0xffffffffffffffffffn), 1605604604175679372656640n);
}

function bigIntBitOr(a, b) {
    let c = a | b;
    if (b) {
        assert(c, a | b);
    }
    return (a | b) + c;
}
noInline(bigIntBitOr);

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitOr(0b1101n, 0b0010n), 30n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitOr(0xaaffffffffffffffffffn, 0xffffffffffffffffffn), 1615049337141418663084030n);
}

function bigIntBitAnd(a, b) {
    let c = a & b;
    if (b) {
        assert(c, a & b);
    }
    return (a & b) + c;
}
noInline(bigIntBitAnd);

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitAnd(0b1101n, 0b0010n), 0n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitAnd(0xaaffffffffffffffffffn, 0xffffffffffffffffffn), 9444732965739290427390n);
}

function bigIntBitXor(a, b) {
    let c = a ^ b;
    if (b) {
        assert(c, a ^ b);
    }
    return (a ^ b) + c;
}
noInline(bigIntBitXor);

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitXor(0b1101n, 0b0010n), 30n);
}

for (let i = 0; i < 100000; i++) {
    assert(bigIntBitXor(0xaaffffffffffffffffffn, 0xffffffffffffffffffn), 1605604604175679372656640n);
}

