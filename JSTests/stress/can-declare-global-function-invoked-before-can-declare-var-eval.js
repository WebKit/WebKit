function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis, "bar", { writable: false, enumerable: true, configurable: false });
Object.preventExtensions(globalThis);

let didThrow = false;
try {
    eval(`
        var foo = 2;
        function bar() {}
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'bar': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(!globalThis.hasOwnProperty("foo"));
