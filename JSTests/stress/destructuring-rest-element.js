function assert(b) {
    if (!b)
        throw new Error("Bad assertion")
}
noInline(assert);

function test(f, count = 1000) {
    for (let i = 0; i < count; i++)
        f();
}

function arr() {
    return [10, 20, 30, 40];
}
noInline(arr);

let o = {};
function arr2() {
    return [10, 20, 30, [40, 50, o]];
}
noInline(arr);

function eq(a, b) {
    // This only works for arrays with nested arrays in them, and numbers or anything else strict equal to each other. 
    if (!(a instanceof Array))
        return a === b;

    if (a.length !== b.length)
        return false;

    for (let i = 0; i < a.length; i++) {
        let e1 = a[i];
        let e2 = b[i];
        if (!eq(e1, e2))
            return false;
    }

    return true;
}

test(function() {
    let [...[...x]] = arr();
    assert(eq(x, arr()));
});

test(function() {
    let [ , , , [...e]] = arr2();
    assert(eq(e, [40, 50, o]));
});

test(function() {
    let [ , , , ...e] = arr2();
    assert(eq(e[0], [40, 50, o]));
});

test(function() {
    let [ , , , ...e] = arr2();
    assert(eq(e[0], [40, 50, o]));
});

function* gen() {
    yield [1,2,3];
    yield 20;
    yield 30;
    yield 40;
}

test(function() {
    let [a, b, ...[...c]] = gen();
    assert(eq(a, [1,2,3]));
    assert(b === 20);
    assert(eq(c, [30, 40]));
});

test(function() {
    let [[a, ...d], b, ...[...c]] = gen();
    assert(a === 1);
    assert(eq(d, [2,3]));
    assert(b === 20);
    assert(eq(c, [30, 40]));
});

let called = false;
function fakeGen() { 
    return {
        [Symbol.iterator]: function() {
            let count = 0;
            return {
                next() {
                    called = true;
                    count++;
                    if (count === 1)
                        return {done: false, value: 50};
                    return {done: true};
                }
            };
        }
    };
}

test(function() {
    called = false;
    let [...x] = fakeGen();
    assert(eq(x, [50]));
    assert(called);
    called = false;
});
