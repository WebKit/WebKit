function assert(a, e, m) {
    if (a !== e)
        throw new Error("Expected to be: " + e + " but got: " + a);
}

function bitNot(a) {
    return ~a;
}
noInline(bitNot);

for (let i = 0; i < 10000; i++) {
    let r = bitNot("0");
    assert(r, -1);
    r = bitNot("1");
    assert(r, -2);
    r = bitNot("-1");
    assert(r, 0);
    r = bitNot("-2");
    assert(r, 1);

    r = bitNot({ valueOf: () => 0 });
    assert(r, -1);
    r = bitNot({ valueOf: () => 1 });
    assert(r, -2);
    r = bitNot({ valueOf: () => -1 });
    assert(r, 0);
    r = bitNot({ valueOf: () => -2 });
    assert(r, 1);
}

