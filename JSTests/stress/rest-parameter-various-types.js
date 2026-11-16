function assert(b) {
    if (!b)
        throw new Error("Bad!");
}
noInline(assert);

function foo(a, b, ...c) {
    return c;
}
noInline(foo);
function bar(a, b, ...c) {
    return c;
}
noInline(bar);

function baz(a, b, ...c) { // allow this to be inlined
    return c;
}

noInline(isNaN);

function test1(f, iters) {
    for (let i = 0; i < iters; i++) {
        let result = f(10, 20, 20.5, 22.45, 23.50);
        assert(result.length === 3);
        assert(result[0] === 20.5)
        assert(result[1] === 22.45)
        assert(result[2] === 23.50);
    }

    let o = {};
    let result = f(10, 20, 20.4, o, 20.2);
    assert(result.length === 3);
    assert(result[0] === 20.4)
    assert(result[1] === o)
    assert(result[2] === 20.2);

    result = f(10, 20, 20.4, 20.45, NaN);
    assert(result.length === 3);
    assert(result[0] === 20.4)
    assert(result[1] === 20.45)
    assert(isNaN(result[2]));
}
test1(foo, 1000);
test1(bar, 10000);
test1(baz, 10000);

function makeTest2() {
    return eval(`${Math.random()};
        ;(function test2(f, iters) {
            let a = 10;
            let b = 20;
            for (let i = 0; i < iters; i++) {
                if (i === iters - 2) {
                    b = {};
                } else if (i === iters - 1) {
                    b = NaN;
                }

                let r = f(a, b);
                assert(r.length === 2);
                assert(r[0] === a || (isNaN(a) && isNaN(r[0])));
                assert(r[1] === b || (isNaN(b) && isNaN(r[1])));
            }
        })`);
}
function f1(...rest) { return rest; }
function f2(...rest) { return rest; }
function f3(...rest) { return rest; }
makeTest2()(f1, 1000);
makeTest2()(f2, 10000);
makeTest2()(f3, 500000);

function test3(f, iters) {
    let o = {};
    for (let i = 0; i < iters; i++) {
        let r = f(i, o, 25);
        assert(r.length === 2 || r.length === 10000);
        assert(r[0] === o);
        assert(r[1] === 25);
        if (r.length === 10000)
            assert(r[9999] === 30);
    }
}

function f11(i, ...rest) {
    if (i === 999)
        rest[9999] = 30;
    return rest;
}
function f22(i, ...rest) {
    if (i === 49999)
        rest[9999] = 30;
    return rest;
}
test3(f11, 1000);
test3(f22, 50000);

