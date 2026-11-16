function assert(b) {
    if (!b)
        throw new Error("bad assertion.");
}

function sameValue(a, b, testname) {
    if (a !== b)
        throw new Error(`${testname}: Expected ${b} but got ${a}`);
}

let targets = [Function, String, Array, Set, Map, WeakSet, WeakMap, RegExp, Number, Promise, Date, Boolean, Error, TypeError, SyntaxError, ArrayBuffer, Int32Array, Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Uint32Array, Float32Array, Float64Array, DataView];
for (let target of targets) {
    const testname = target.name;
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
            if (target === Promise)
                new proxy(function() {});
            if (target === DataView)
                new proxy(new ArrayBuffer(64));
            else
                new proxy;
        } catch(e) {
            threw = true;
            sameValue(e, error, testname);
            error = null;
        }
        sameValue(threw, true, testname);
        sameValue(called, true, testname);
        called = false;
    }
}
