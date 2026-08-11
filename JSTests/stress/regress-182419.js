function assertEqual(actual, expected) {
    if (actual != expected)
        throw "FAILED: expect " + expected + ", actual " + actual;
}

function test(index1, index2) {
    function baz(a, b, c, ...args) {
        return [args.length, args[index1], args[index2]];
    }
    function jaz(...args) {
        return baz.apply(null, args);
    }
    noInline(jaz);

    function check() {
        let [length, a, b] = jaz();
        assertEqual(length, 0);
        assertEqual(a, undefined);
        assertEqual(b, undefined);
    }

    for (let i = 0; i < 20000; i++) {
        check();
    }
}

test(0, 1);
test(0x7fffffff, 0);
