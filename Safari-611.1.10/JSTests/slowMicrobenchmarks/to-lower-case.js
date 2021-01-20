function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);

let tests = [
    ["foo", "foo"],
    ["foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", "foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"],
];

function foo(a) {
    return a.toLowerCase();
}
noInline(foo);

function bar(a) {
    a.toLowerCase();
    assert(true); // side effects
    a.toLowerCase();
}
noInline(bar);

let start = Date.now();
for (let i = 0; i < 500000; i++) {
    for (let i = 0; i < tests.length; i++) {
        let test = tests[i][0];
        let result = tests[i][1];
        assert(foo(test) === result);
        bar(test);
    }
}

const verbose = false;
if (verbose)
    print(Date.now() - start);
