var createBuiltin = $vm.createBuiltin;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

let list = [
    "Int8Array",
    "Uint8Array",
    "Uint8ClampedArray",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float32Array",
    "Float64Array",
    "BigInt64Array",
    "BigUint64Array",
];

for (let constructorName of list) {
    let builtin = createBuiltin(`(function (a) {
        return @${constructorName};
    })`);
    let constructor = builtin();
    shouldBe(constructor.name, constructorName);
    shouldBe(constructor, globalThis[constructorName]);
}
