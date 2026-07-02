function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Failed assertion: actual " + actual + " should be " + expected);
}

function readTypedArraySymbolSpeciesGetter(constructor) {
    return Object.getOwnPropertyDescriptor(Object.getPrototypeOf(constructor), Symbol.species).get;
}

function runTest(constructors) {
    for (let i = 0; i < constructors.length; i++) {
        for (let j = 0; j < constructors.length; j++) {
            if (i !== j) {
                shouldBe(readTypedArraySymbolSpeciesGetter(constructors[i]), readTypedArraySymbolSpeciesGetter(constructors[j]));
            }
        }
    }
}

const constructors = [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array
];

runTest(constructors);
