function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function test(s) {
    let len = 4;
    var a = new Array(len);
    for (var i = 0; i < len; i++) {
        a[i] = s[i];
    }
    // FIXME: why read a[0] is not eliminated but a[1] does?
    s[0] = a[0] ^ a[1];
    return s;
}
noInline(test);

let expected;
for (let i = 0; i < 1e5; i++) {
    let a = [0, 0];
    let res = test(a);
    if (i == 0)
        expected = res;
    assert(res, expected);
}
