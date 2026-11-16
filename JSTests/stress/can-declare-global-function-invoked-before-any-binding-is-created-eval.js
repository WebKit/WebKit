function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

Object.defineProperty(globalThis, "a", { writable: true, enumerable: false, configurable: false });

let didThrow = false;
try {
    eval(`
        var d;
        function a() {}
        function b() {}
        var c;
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global function 'a': property must be either configurable or both writable and enumerable");
}

assert(didThrow);
assert(["b", "c", "d"].every(k => !globalThis.hasOwnProperty(k)));
