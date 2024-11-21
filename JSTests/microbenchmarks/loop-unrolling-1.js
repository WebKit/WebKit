function assert(a) {
    for (const e of a) {
        if (e != 2)
            throw new Error("bad val=" + e);
    }
}

function test(a) {
    for (let i = 0; i < 4; i++) {
        a[i] = a[i] + 1;
        a[i] = a[i] + 1;
    }
    return a;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
    let a = [0, 0, 0, 0];
    assert(test(a));
}

