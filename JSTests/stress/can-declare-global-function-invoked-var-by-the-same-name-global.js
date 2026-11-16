function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

let fooSetCalls = 0;
Object.defineProperty(globalThis, "foo", { set() { fooSetCalls++; }, enumerable: true, configurable: false });

let didThrow = false;
try {
    eval(`
        var foo = 2;
        function foo() {}
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'foo': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(fooSetCalls === 0);
