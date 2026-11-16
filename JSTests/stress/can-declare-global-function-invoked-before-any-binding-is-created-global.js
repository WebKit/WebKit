function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis, "b", { writable: false, enumerable: true, configurable: false });

let didThrow = false;
try {
    $262.evalScript(`
        var d;
        function a() {}
        function b() {}
        var c;
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'b': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(["a", "c", "d"].every(k => !globalThis.hasOwnProperty(k)));
