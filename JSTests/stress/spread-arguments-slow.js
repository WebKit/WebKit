function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

// DirectArguments
function test1(a, b, c) {
    arguments.length = 1;
    return [...arguments]
}
noInline(test1);

// ScopedArguments
function test2(a, b, c) {
    function hello() { return b + c; }
    globalThis.hello = hello;
    arguments.length = 1;
    return [...arguments];
}
noInline(test2);

// ClonedArguments
function test3(a, b, c) {
    "use strict";
    arguments.length = 1;
    return [...arguments]
}
noInline(test3);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(test1('a', 1, 2)), `["a"]`);
    shouldBe(JSON.stringify(test2('a', 1, 2)), `["a"]`);
    shouldBe(JSON.stringify(test3('a', 1, 2)), `["a"]`);
}
