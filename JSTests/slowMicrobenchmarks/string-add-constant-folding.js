function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

let tests = [];
function test(f) {
    noInline(f);
    tests.push(f);
}

function runTests() {
    let start = Date.now();
    for (let f of tests) {
        for (let i = 0; i < 40000; i++)
            f();
    }
    const verbose = false;
    if (verbose)
        print(Date.now() - start);
}

function add(a,b) { return a + b; }
noInline(add);

test(function() {
    let a = "foo";
    let b = 20;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = null;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = undefined;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 20.81239012821;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = true;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = false;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = NaN;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = -0;
    let b = "foo";
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 0.0;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = Infinity;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = -Infinity;
    let b = "foo";
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e10;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e-10;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e5;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e-5;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e-40;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

test(function() {
    let a = "foo";
    let b = 1e40;
    assert(a + b === add(a, b));
    assert(b + a === add(b, a));
});

runTests();
