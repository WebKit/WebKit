function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

globalThis.a = 1;
globalThis.b = 2;
Object.preventExtensions(globalThis);

let didThrow = false;
try {
    eval(`
        var d = 4;
        function a() {}
        function b() {}
        var c = 3;
    `);
} catch (err) {
    didThrow = true;
    assert(err.toString() === "TypeError: Can't declare global variable 'c': global object must be extensible");
}

assert(didThrow);
assert(a === 1);
assert(b === 2);
