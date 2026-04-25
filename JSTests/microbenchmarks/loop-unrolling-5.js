function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function test(s) {
    var a = new Array(4);
    for (var i = 0; i < 4; i++) {
        a[i] = s[i] & 0x80;
    }

    s[0] = a[0] ^ a[1] ^ a[2];
    s[1] = a[1] ^ a[2] ^ a[3];
    s[2] = a[2] ^ a[3] ^ a[0];
    s[3] = a[3] ^ a[0] ^ a[1];

    s[4] = a[0] ^ a[1] ^ a[2];
    s[5] = a[1] ^ a[2] ^ a[3];
    s[6] = a[2] ^ a[3] ^ a[0];
    s[7] = a[3] ^ a[0] ^ a[1];
    return s;
}
noInline(test);

let expected;
for (let i = 0; i < 1e5; i++) {
    let a = [1, 2, 3, 4, 5, 6, 7, 8];
    let res = test(a);
    if (i == 0)
        expected = res;
    assert(res, expected);
}

