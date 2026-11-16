// Symbol.for observes VM::symbolImplToSymbolMap, one of the WeakGCMaps, directly: if a reachable
// entry is dropped, the next lookup builds a fresh Symbol cell for the same key and the two are not
// identical. Eden and full collections reconcile those maps by different paths, so exercise both.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}`);
}

const symbols = [];
for (let i = 0; i < 128; ++i)
    symbols.push(Symbol.for(`key-${i}`));

for (let i = 0; i < 32; ++i) {
    // Churn structure transitions, prototype structures, and interned strings, so that the weak
    // tables gain entries between collections.
    for (let j = 0; j < 512; ++j) {
        const object = {};
        object[`p${j & 31}`] = j;
        object.tail = j;
        Object.create(object);
        `a,b,c-${j}`.split(",");
    }

    if (i & 1)
        edenGC();
    else
        gc();

    for (let j = 0; j < symbols.length; ++j)
        shouldBe(Symbol.for(`key-${j}`), symbols[j]);
}
