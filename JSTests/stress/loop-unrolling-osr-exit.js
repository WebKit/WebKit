//@ runDefault("--useDollarVM=1")
function assert(actual, expected) {
    for (let i = 0; i < actual.length; i++) {
        if (actual[i] != expected[i])
            throw new Error("bad actual=" + actual[i] + " but expected=" + expected[i]);
    }
}

function func1(a) {
    for (let i = 0; i < 4;) {
        a[i] = 1;
        // FIXME: ForceOSRExit can break the loop graph in CFG simplification.
        // We may need other ways to trigger OSR exits for loop unrolling.
        if ($vm.ftlTrue()) OSRExit();
        i++;
    }
    return a;
}
noInline(func1);

function func2(a) {
    for (let i = 0; i < 4;) {
        a[i] = 1;
        i++;
        if ($vm.ftlTrue()) OSRExit();
    }
    return a;
}
noInline(func2);

function func3(a) {
    for (let i = 0; i < 4;) {
        if ($vm.ftlTrue()) OSRExit();
        a[i] = 1;
        i++;
    }
    return a;
}
noInline(func3);

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

test(func1);
// test(func2);
// test(func3);

