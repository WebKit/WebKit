function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis, "foo", { get() {}, enumerable: true, configurable: false });
Object.preventExtensions(globalThis);

let didThrow = false;
try {
    $262.evalScript(`
        var bar = 1;
        function foo() {}
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'foo': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(!globalThis.hasOwnProperty("bar"));
