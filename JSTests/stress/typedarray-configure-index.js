typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

function assert(cond) {
    if (!cond)
        throw new Error("bad assertion!");
}

function assertThrows(thunk, error) {
    let failed = true;
    try {
        thunk();
    } catch (e) {
        if (error && e != error)
            throw new Error("bad assertion!");
        failed = false;
    }
    if (failed)
        throw new Error("bad assertion!");
}

function makeDescriptor(accessor, configurable, writable, enumerable) {
    let o = {writable, configurable, enumerable}
    if (accessor)
        o.get = () => 1;
    else
        o.value = 1;
    return o;
}

let bools = [true, false];

function test(array, a, c, error ) {
    for (w of bools) {
        for (e of bools) {
            assertThrows(() => Object.defineProperty(a, 0, makeDescriptor(a, c, w, e), error));
        }
    }
}

function foo() {
    for (constructor of typedArrays) {
        let a = new constructor(10);
        Object.defineProperty(a, 0, makeDescriptor(false, false, true, true));
        assert(a[0] === 1);
        assertThrows(() => Object.defineProperty(a, 0, makeDescriptor(false, false, true, false), "TypeError: Attempting to store non-enumerable or non-writable indexed property on a typed array."));
        assertThrows(() => Object.defineProperty(a, 0, makeDescriptor(false, false, false, false), "TypeError: Attempting to store non-enumerable or non-writable indexed property on a typed array."));
        assertThrows(() => Object.defineProperty(a, 0, makeDescriptor(false, false, false, true), "TypeError: Attempting to store non-enumerable or non-writable indexed property on a typed array."));

        test(a, false, true, "TypeError: Attempting to configure non-configurable property.");
        for (c of bools) {
            test(a, true, c, "TypeError: Attempting to store accessor indexed property on a typed array.")
        }
    }
}

for (let i = 0; i < 100; i++)
    foo();
