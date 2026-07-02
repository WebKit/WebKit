function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function test(a) {
    for (let i = 0; i < 4; i++) {
        if (i % 2 == 0)
            a[i] = 1;
        else
            a[i] = 2;
    }
    return a;
}
noInline(test);

let expected;
for (let i = 0; i < 1e5; i++) {
    let a = [0, 0, 0, 0];
    let res = test(a);
    if (i == 0)
        expected = res;
    assert(res, expected);
}

