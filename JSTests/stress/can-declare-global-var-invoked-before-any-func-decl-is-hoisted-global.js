function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

globalThis.bar = 1;
Object.preventExtensions(globalThis);

let didThrow = false;
try {
    $262.evalScript(`
        if (true) {
            function bar() {}
        }

        var foo;
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global variable 'foo': global object must be extensible");
}

assert(didThrow);
assert(bar === 1);
