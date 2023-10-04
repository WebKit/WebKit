function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

globalThis.foo = 1;
Object.preventExtensions(globalThis);

let didThrow = false;
try {
    eval(`
        if (true) {
            function foo() {}
        }

        var bar;
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global variable 'bar': global object must be extensible");
}

assert(didThrow);
assert(foo === 1);
