function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

let tests = [
    ["FOO", "foo"],
    ["fff\u00C2", "fff\u00E2"],
    ["foo", "foo"],
    ["foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", "foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"],
    ["BaR", "bar"],
    ["FOO\u00A9", "foo\u00A9"],
    ["#$#$", "#$#$"],
    ["&&&\u00A9", "&&&\u00A9"],
    ["&&&\u00C2", "&&&\u00E2"],
    ["ABC\u0100", "abc\u0101"],
];

function foo(a) {
    return a.toLowerCase();
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    for (let i = 0; i < tests.length; i++) {
        let test = tests[i][0];
        let result = tests[i][1];
        assert(foo(test) === result);
    }
}
