function shouldNotBe(actual, expected) {
    if (actual === expected)
        throw new Error("Failed assertion: actual " + actual + " should not be " + expected);
}

function readSymbolSpeciesGetter(constructor) {
    return Object.getOwnPropertyDescriptor(constructor, Symbol.species).get;
}

function runTest(constructors) {
    for (let i = 0; i < constructors.length; i++) {
        for (let j = 0; j < constructors.length; j++) {
            if (i !== j) {
                shouldNotBe(readSymbolSpeciesGetter(constructors[i]), readSymbolSpeciesGetter(constructors[j]));
            }
        }
    }
}

const constructors = [
    Array,
    RegExp,
    Set,
    Map,
    Promise,
    ArrayBuffer,
    SharedArrayBuffer,
    Object.getPrototypeOf(Uint16Array)
];

runTest(constructors);
