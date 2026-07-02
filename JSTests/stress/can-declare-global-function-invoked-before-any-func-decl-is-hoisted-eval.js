function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

let fooSetCalls = 0;
Object.defineProperty(globalThis, "foo", { get() {}, set() { fooSetCalls++; }, enumerable: true, configurable: false });

let didThrow = false;
try {
    eval(`
        if (true) {
            function bar() {}
        }

        function foo() {}
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'foo': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(fooSetCalls === 0);
assert(!globalThis.hasOwnProperty("bar"));
