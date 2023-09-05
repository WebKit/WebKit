function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis, "foo", { value: 1, writable: false, enumerable: false, configurable: false });

let didThrow = false;
try {
    $262.evalScript(`
        var foo = 2;
        function foo() {}
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'foo': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(foo === 1);
