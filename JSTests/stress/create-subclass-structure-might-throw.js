function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

let targets = [Function, String, Array, Set, Map, WeakSet, WeakMap, RegExp, Number, Promise, Date, Boolean, Error, TypeError, SyntaxError, ArrayBuffer, Int32Array, Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Uint32Array, Float32Array, Float64Array, DataView];
for (let target of targets) {
    let error = null;
    let called = false;
    let handler = {
        get: function(theTarget, propName) {
            assert(propName === "prototype");
            error = new Error;
            called = true;
            throw error;
        }
    };

    let proxy = new Proxy(target, handler);

    for (let i = 0; i < 500; i++) {
        let threw = false;
        try {
            new proxy;
        } catch(e) {
            threw = true;
            assert(e === error);
            error = null;
        }
        assert(threw);
        assert(called);
        called = false;
    }
}
