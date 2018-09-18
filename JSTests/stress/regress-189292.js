//@ runDefault

function assert(a, b) {
    if (a != b)
        throw "FAIL";
}

function test(script) {
    try {
        eval(script);
    } catch (e) {
        return e;
    }
}

assert(test("class C1 { async constructor() { } }"), "SyntaxError: Cannot declare an async method named 'constructor'.");
assert(test("class C1 { *constructor() { } }"), "SyntaxError: Cannot declare a generator function named 'constructor'.");
assert(test("class C1 { async *constructor() { } }"), "SyntaxError: Cannot declare an async generator method named 'constructor'.");
