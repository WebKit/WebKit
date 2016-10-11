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

runTests();
