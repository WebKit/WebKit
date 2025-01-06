function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

let int_max = 2 ** 31 - 1;
let update = 2 ** 30;
let update2 = 2 ** 30 - 1;

function overflow(a) {
    for (let i = 0; i <= int_max; i = i + update) {
        a[i % 4] = i;
    }
    return a;
}
noInline(overflow);

function underflow(a) {
    for (let i = 0; i >= -int_max; i = i - update2) {
        a[Math.abs(i) % 4] = i;
    }
    return a;
}
noInline(underflow);

function test(func) {
    let expected;
    for (let i = 0; i < 1e5; i++) {
        let a = [0, 0, 0, 0];
        let res = func(a);
        if (i == 0)
            expected = res;
        assert(res, expected);
    }
}

test(overflow);
test(underflow);
