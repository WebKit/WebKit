function shouldThrow(func, errorName) {
    try {
        func();
    } catch (e) {
        if (e.name === errorName)
            return;
        throw new Error("threw " + e.name + ": " + e.message + ", expected " + errorName);
    }
    throw new Error("expected " + errorName);
}

let newTarget = new Proxy(function() {}, {
    get() { throw new EvalError("prototype read"); }
});

for (let length of [-1, 2 ** 53, Infinity]) {
    shouldThrow(() => Reflect.construct(ArrayBuffer, [length], newTarget), "RangeError");
    shouldThrow(() => Reflect.construct(SharedArrayBuffer, [length], newTarget), "RangeError");
}

shouldThrow(() => Reflect.construct(ArrayBuffer, [0, { maxByteLength: -1 }], newTarget), "RangeError");
shouldThrow(() => new ArrayBuffer(2 ** 32, { maxByteLength: 1 }), "RangeError");
shouldThrow(() => new SharedArrayBuffer(2 ** 32, { maxByteLength: 1 }), "RangeError");
shouldThrow(() => Reflect.construct(ArrayBuffer, [1.5], newTarget), "EvalError");

let optionReads = 0;
shouldThrow(() => Reflect.construct(ArrayBuffer, [-1, { get maxByteLength() { optionReads++; return 1; } }], newTarget), "RangeError");
if (optionReads)
    throw new Error("maxByteLength was read before ToIndex");

let buffer = Reflect.construct(ArrayBuffer, [4, { maxByteLength: 8 }], ArrayBuffer);
if (buffer.byteLength !== 4 || buffer.maxByteLength !== 8)
    throw new Error("resizable ArrayBuffer");

let truncated = new ArrayBuffer(1.9, { maxByteLength: 1 });
if (truncated.byteLength !== 1)
    throw new Error("ToIndex truncation");
