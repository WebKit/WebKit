function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

let tests = [
    ["foo", "FOO"],
    ["FOO", "FOO"],
    ["BaR", "BAR"],
    ["fff\u00E2", "FFF\u00C2"],
    ["foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo", "FOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO"],
    ["FOO\u00A9", "FOO\u00A9"],
    ["#$#$", "#$#$"],
    ["&&&\u00A9", "&&&\u00A9"],
    ["&&&\u00E2", "&&&\u00C2"],
    ["abc\u0101", "ABC\u0100"],
    ["\u00DF", "SS"],
    ["123", "123"],
    ["", ""],
];

function foo(a) {
    return a.toUpperCase();
}
noInline(foo);

for (let i = 0; i < testLoopCount; i++) {
    for (let j = 0; j < tests.length; j++) {
        let test = tests[j][0];
        let result = tests[j][1];
        assert(foo(test) === result);
    }
}
