//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.001")
let typedArrayConstructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array,
];

function test(typedArrayConstructor) {
    function opt(f) {
        let shouldThrow = f != typedArrayConstructor;
        try {
            f(typedArrayConstructor);
        } catch (e) {
            shouldThrow = e.toString() == `TypeError: calling ${typedArrayConstructor.name} constructor without new is invalid`;
        }
        if (!shouldThrow)
            throw new Error("bad!");
    }

    for (let i = 0; i < 10; i++)
        opt(opt);
}

for (let i = 0; i < typedArrayConstructors.length; i++)
    test(typedArrayConstructors[i]);
